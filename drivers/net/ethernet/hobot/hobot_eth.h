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

#ifndef __HOBOT_ETH_H_
#define __HOBOT_ETH_H_

#include <linux/stddef.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <asm/io.h>
#include <linux/clk.h>
#include <linux/ptp_clock_kernel.h>

#include <linux/pinctrl/consumer.h>

#ifdef CONFIG_HOBOT_DMC_CLK
#include <soc/hobot/hobot_bus.h>
#endif

#include "hobot_mmc.h"

#define XJ3_MAX_RX_QUEUES 8
#define XJ3_MAX_TX_QUEUES 8

#define HOBOT_RX_FRAMES 2
#define MAX_DMA_RIWT 0x300ff
#define MIN_DMA_RIWT 0x1

#define HOBOT_COAL_TX_TIMER 1000
#define HOBOT_MAX_COAL_TX_TICK 100000
#define HOBOT_TX_MAX_FRAMES 250
#define HOBOT_TX_FRAMES 2

#define XJ3_GET_ENTRY(x, size) ((x + 1) & (size - 1))
#define DRIVER_VERSION			"0.9"
struct xj3_rx_routing {
	u32 reg_mask;
	u32 reg_shift;
};


struct xj3_extra_stats {
	/* Transmit errors */
	unsigned long tx_underflow ____cacheline_aligned;
	unsigned long tx_carrier;
	unsigned long tx_losscarrier;
	unsigned long vlan_tag;
	unsigned long tx_deferred;
	unsigned long tx_vlan;
	unsigned long tx_jabber;
	unsigned long tx_frame_flushed;
	unsigned long tx_payload_error;
	unsigned long tx_ip_header_error;
	/* Receive errors */
	unsigned long rx_desc;
	unsigned long sa_filter_fail;
	unsigned long overflow_error;
	unsigned long ipc_csum_error;
	unsigned long rx_collision;
	unsigned long rx_crc_errors;
	unsigned long dribbling_bit;
	unsigned long rx_length;
	unsigned long rx_mii;
	unsigned long rx_multicast;
	unsigned long rx_gmac_overflow;
	unsigned long rx_watchdog;
	unsigned long da_rx_filter_fail;
	unsigned long sa_rx_filter_fail;
	unsigned long rx_missed_cntr;
	unsigned long rx_overflow_cntr;
	unsigned long rx_vlan;
	/* Tx/Rx IRQ error info */
	unsigned long tx_undeflow_irq;
	unsigned long tx_process_stopped_irq;
	unsigned long tx_jabber_irq;
	unsigned long rx_overflow_irq;
	unsigned long rx_buf_unav_irq;
	unsigned long rx_process_stopped_irq;
	unsigned long rx_watchdog_irq;
	unsigned long tx_early_irq;
	unsigned long fatal_bus_error_irq;
	/* Tx/Rx IRQ Events */
	unsigned long rx_early_irq;
	unsigned long threshold;
	unsigned long tx_pkt_n;
	unsigned long rx_pkt_n;
	unsigned long normal_irq_n;
	unsigned long rx_normal_irq_n;
	unsigned long napi_poll;
	unsigned long tx_normal_irq_n;
	unsigned long tx_clean;
	unsigned long tx_set_ic_bit;
	unsigned long irq_receive_pmt_irq_n;
	/* MMC info */
	unsigned long mmc_tx_irq_n;
	unsigned long mmc_rx_irq_n;
	unsigned long mmc_rx_csum_offload_irq_n;
	/* EEE */
	unsigned long irq_tx_path_in_lpi_mode_n;
	unsigned long irq_tx_path_exit_lpi_mode_n;
	unsigned long irq_rx_path_in_lpi_mode_n;
	unsigned long irq_rx_path_exit_lpi_mode_n;
	unsigned long phy_eee_wakeup_error_n;
	/* Extended RDES status */
	unsigned long ip_hdr_err;
	unsigned long ip_payload_err;
	unsigned long ip_csum_bypassed;
	unsigned long ipv4_pkt_rcvd;
	unsigned long ipv6_pkt_rcvd;
	unsigned long no_ptp_rx_msg_type_ext;
	unsigned long ptp_rx_msg_type_sync;
	unsigned long ptp_rx_msg_type_follow_up;
	unsigned long ptp_rx_msg_type_delay_req;
	unsigned long ptp_rx_msg_type_delay_resp;
	unsigned long ptp_rx_msg_type_pdelay_req;
	unsigned long ptp_rx_msg_type_pdelay_resp;
	unsigned long ptp_rx_msg_type_pdelay_follow_up;
	unsigned long ptp_rx_msg_type_announce;
	unsigned long ptp_rx_msg_type_management;
	unsigned long ptp_rx_msg_pkt_reserved_type;
	unsigned long ptp_frame_type;
	unsigned long ptp_ver;
	unsigned long timestamp_dropped;
	unsigned long av_pkt_rcvd;
	unsigned long av_tagged_pkt_rcvd;
	unsigned long vlan_tag_priority_val;
	unsigned long l3_filter_match;
	unsigned long l4_filter_match;
	unsigned long l3_l4_filter_no_match;
	/* PCS */
	unsigned long irq_pcs_ane_n;
	unsigned long irq_pcs_link_n;
	unsigned long irq_rgmii_n;
	unsigned long pcs_link;
	unsigned long pcs_duplex;
	unsigned long pcs_speed;
	/* debug register */
	unsigned long mtl_tx_status_fifo_full;
	unsigned long mtl_tx_fifo_not_empty;
	unsigned long mmtl_fifo_ctrl;
	unsigned long mtl_tx_fifo_read_ctrl_write;
	unsigned long mtl_tx_fifo_read_ctrl_wait;
	unsigned long mtl_tx_fifo_read_ctrl_read;
	unsigned long mtl_tx_fifo_read_ctrl_idle;
	unsigned long mac_tx_in_pause;
	unsigned long mac_tx_frame_ctrl_xfer;
	unsigned long mac_tx_frame_ctrl_idle;
	unsigned long mac_tx_frame_ctrl_wait;
	unsigned long mac_tx_frame_ctrl_pause;
	unsigned long mac_gmii_tx_proto_engine;
	unsigned long mtl_rx_fifo_fill_level_full;
	unsigned long mtl_rx_fifo_fill_above_thresh;
	unsigned long mtl_rx_fifo_fill_below_thresh;
	unsigned long mtl_rx_fifo_fill_level_empty;
	unsigned long mtl_rx_fifo_read_ctrl_flush;
	unsigned long mtl_rx_fifo_read_ctrl_read_data;
	unsigned long mtl_rx_fifo_read_ctrl_status;
	unsigned long mtl_rx_fifo_read_ctrl_idle;
	unsigned long mtl_rx_fifo_ctrl_active;
	unsigned long mac_rx_frame_ctrl_fifo;
	unsigned long mac_gmii_rx_proto_engine;
	/* TSO */
	unsigned long tx_tso_frames;
	unsigned long tx_tso_nfrags;
};

struct stmmac_stats {
	char stat_name[ETH_GSTRING_LEN];
	int sizeof_stat;
	int stat_offset;
};


struct dma_features {
	unsigned int mbps_10_100;
	unsigned int mbps_1000;
	unsigned int half_duplex;
	unsigned int hash_filter;
	unsigned int multi_addr;
	unsigned int pcs;
	unsigned int sma_mdio;
	unsigned int pmt_remote_wake_up;
	unsigned int pmt_magic_frame;
	unsigned int rmon;
	/* IEEE 1588-2002 */
	unsigned int time_stamp;
	/* IEEE 1588-2008 */
	unsigned int atime_stamp;
	/* 802.3az - Energy-Efficient Ethernet (EEE) */
	unsigned int eee;
	unsigned int av;
	unsigned int tsoen;
	/* TX and RX csum */
	unsigned int tx_coe;
	unsigned int rx_coe;
	unsigned int rx_coe_type1;
	unsigned int rx_coe_type2;
	unsigned int rxfifo_over_2048;
	/* TX and RX number of channels */
	unsigned int number_rx_channel;
	unsigned int number_tx_channel;
	/* TX and RX number of queues */
	unsigned int number_rx_queues;
	unsigned int pps_out_num;

	unsigned int number_tx_queues;
	/* Alternate (enhanced) DESC mode */
	unsigned int enh_desc;
	/* TX and RX FIFO sizes */
	unsigned int tx_fifo_size;
	unsigned int rx_fifo_size;

	/*TSN feature*/
	unsigned int tsn;

	/*frame preemption*/
///	unsigned int tsn_frame_preemption;
	unsigned int fpesel;

	/*Enhancements to Scheduling Trafic*/
	//unsigned int tsn_enh_sched_traffic;
	unsigned int estsel;

	unsigned int estwid;
	unsigned int estdep;

	unsigned int asp;
	unsigned int frpsel;
	unsigned int frpbs;
	unsigned int frpes;
	unsigned int tbssel;
};


struct xj3_resource {
	void __iomem *addr;
	const char *mac;
	int irq;
	int tx_irq;
	int rx_irq;
};
struct dma_desc {
	__le32 des0;
	__le32 des1;
	__le32 des2;
	__le32 des3;
};


struct dma_ext_desc {
	struct dma_desc basic;
	__le32 des4;
	__le32 des5;
	__le32 des6;
	__le32 des7;
};


struct xj3_rx_queue {
	u32 rx_count_frames;
	u32 queue_index;
	struct xj3_priv *priv_data;

	struct dma_ext_desc *dma_erx ____cacheline_aligned_in_smp;
	struct dma_desc *dma_rx;
	struct sk_buff **rx_skbuff;
	dma_addr_t *rx_skbuff_dma;

	unsigned int cur_rx;
	unsigned int dirty_rx;
	unsigned int refill_rx;
	unsigned int reschedule_fill_flag;
	dma_addr_t dma_rx_phy;
	u32 rx_tail_addr;
	struct napi_struct napi;
	struct delayed_work work;

	u32 rx_zeroc_thresh;
};

struct xj3_tx_info {
	dma_addr_t buf;
	bool map_as_page;
	unsigned len;
	bool last_segment;
	bool is_jumbo;

};
struct xj3_tx_queue {
	u32 queue_index;
	struct xj3_priv *priv_data;
	struct dma_ext_desc *dma_etx ____cacheline_aligned_in_smp;
	struct dma_desc *dma_tx;

	struct sk_buff **tx_skbuff;
	struct xj3_tx_info *tx_skbuff_dma;
	unsigned int cur_tx;
	unsigned int dirty_tx;
	dma_addr_t dma_tx_phy;
	u32 tx_tail_addr;

	u32 tx_count_frames;
	struct timer_list txtimer;
	struct napi_struct tx_napi;
};


struct xj3_rxq_cfg {
	u8 mode_to_use;
	u32 chan;
	u8 pkt_route;
	bool use_prio;
	u32 prio;
};

struct xj3_txq_cfg {
	u32 weight;
	u8 mode_to_use;

	u32 send_slope;
	u32 idle_slope;
	u32 high_credit;
	u32 low_credit;
	u32 percentage;
	bool use_prio;
	u32 prio;

};
#define STMMAC_IOCTL_EST_GCL_MAX_ENTRIES 1024

#define STMMAC_EST_GCL_MAX_ENTRIES		1024


struct xj3_est_cfg {
	__u32 cmd;
	__u32 enabled;
	__u32 estwid;
	__u32 estdep;
	u32 btr_offset[2];
	u32 ctr[2];
	u32 ter;
	u32 gcl[STMMAC_IOCTL_EST_GCL_MAX_ENTRIES];
	u32 gcl_size;
};

struct xj3_fpe_cfg {
	__u32 cmd;
	__u32 enabled;

};




struct xj3_dma_cfg {
	int pbl;
	int txpbl;
	int rxpbl;

	bool pblx8;
	int fixed_burst;
	int mixed_burst;
	bool aal;

	u32 read_requests;
	u32 write_requests;
	u32 burst_map;
	bool en_lpi;
	u32 interrupt_mode;

};

struct xj3_mdio_bus_data {
	int (*phy_reset)(void *priv);
	unsigned int phy_mask;
	int *irqs;
	int probed_phy_irq;
	int reset_gpio, active_low;
	u32 delays[3];
};

#define AXI_BLEN 7
struct xj3_axi {
	bool axi_lpi_en;
	bool axi_xit_frm;
	u32 axi_wr_osr_lmt;
	u32 axi_rd_osr_lmt;
	bool axi_kbbe;
	u32 axi_blen[AXI_BLEN];
	bool axi_fb;
	bool axi_mb;
	bool axi_rb;
};

#define XJ3_PPS_MAX 4
struct xj3_pps_cfg {
	bool available;
	struct timespec64 start;
	struct timespec64 period;

};


#define STMMAC_PPS_MAX_NUM 4
struct stmmac_pps_cfg {
	bool enable;
	u32 ctrl_cmd;
	u32 trgtmodsel;
	u32 target_time[2];
	int interval;
	u32 width;

};

struct plat_config_data {
	struct device_node *phy_node;
	struct device_node *mdio_node;
	phy_interface_t interface;
	void __iomem *sub_addr;

	int max_speed;
	int bus_id;
	int phy_addr;
	int clk_csr;
	int has_gmac4;
	int pmt;
	int maxmtu;
	int phy_reset_extra_us;
	struct xj3_axi *axi;

	u32 use_riwt;
    struct pinctrl *pinctrl;
    struct pinctrl_state *pin_eth_mux;

	struct xj3_mdio_bus_data *mdio_bus_data;

	struct xj3_rxq_cfg rx_queues_cfg[XJ3_MAX_RX_QUEUES];
	struct xj3_txq_cfg tx_queues_cfg[XJ3_MAX_TX_QUEUES];
	struct xj3_dma_cfg *dma_cfg;

	struct clk *xj3_mac_pre_div_clk;
	struct clk *xj3_mac_div_clk;
	struct clk *xj3_phy_ref_clk;
	struct clk *xj3_pclk;
	struct clk *clk_ptp_ref;

	struct gpio_desc *phyreset;
	struct reset_control *rst;

	u32 rx_queues_to_use;
	u32 tx_queues_to_use;
	u8 rx_sched_algorithm;
	u8 tx_sched_algorithm;


	int rx_fifo_size;
	int tx_fifo_size;

	int rx_coe;
	int tx_coe;
	bool tso_en;

	int enh_desc;

	int force_no_rx_coe;
	int force_no_tx_coe;

	bool est_en;
	struct xj3_est_cfg est_cfg;
	bool fp_en;
	bool tbssel;

	unsigned int clk_ptp_rate;
	unsigned int speed_100M_max_rx_delay;
	unsigned int speed_100M_max_tx_delay;
	unsigned int speed_1G_max_rx_delay;
	unsigned int speed_1G_max_tx_delay;
	unsigned int mac_tx_delay;
	unsigned int mac_rx_delay;
	unsigned int cdc_delay;


	int force_sf_dma_mode;
	int force_thresh_dma_mode;
	bool en_tx_lpi_clockgating;
	int mac_port_sel_speed;
	struct clk *xj3_clk;


	struct stmmac_pps_cfg pps_cfg[STMMAC_PPS_MAX_NUM];

};

#define XJ3_MAX_DMA_CH 4
#define IRQ_MAX_NAME 10
struct xj3_priv {
	u32 tx_count_frames;
	u32 rx_coal_frames;
	u32 tx_coal_frames;

    char txirq_name[IRQ_MAX_NAME];
    char rxirq_name[IRQ_MAX_NAME];
    struct timer_list txtimer;
	u32 tx_coal_timer;
	u32 use_riwt;
	u32 rx_riwt;

	void __iomem *ioaddr;

	struct net_device *dev;
	struct device *device;
	struct mii_bus *mii;

	struct xj3_rx_queue rx_queue[XJ3_MAX_RX_QUEUES];
	struct xj3_tx_queue tx_queue[XJ3_MAX_TX_QUEUES];


	struct plat_config_data *plat;

	int hw_cap_support;
	struct dma_features dma_cap;
    struct hobot_counters mmc;

	u32 csr_val;

	spinlock_t lock;
	spinlock_t state_lock;

	bool link;
	int speed;
	int duplex;
	unsigned int pause;
	unsigned int flow_ctrl_rx;
	unsigned int flow_ctrl_tx;
	unsigned int flow_current_rx;
	unsigned int flow_current_tx;
	unsigned int rx_csum;
	int clk_csr;
    char sysfs_phy_addr;

	unsigned int dma_buf_sz;

	int extend_desc;

	void __iomem *mmcaddr;
	void __iomem *ptpaddr;

	u32 mss;
	bool tso;




	struct xj3_extra_stats xstats ____cacheline_aligned_in_smp;

	bool tx_path_in_lpi_mode;
	int hwts_rx_en;
	int hwts_tx_en;
	u32 msg_enable;
	u32 adv_ts;

	struct hwtstamp_config tstamp_config;
	unsigned int default_addend;
	struct ptp_clock *ptp_clock;
	struct ptp_clock_info ptp_clock_ops;
	spinlock_t ptp_lock;
	u32 sub_second_inc;
	u32 systime_flags;
	struct xj3_pps_cfg pps[XJ3_PPS_MAX];

	u8 pcp_hi;
	u8 pcp_lo;
	u8 tsn_ready:1;
	u8 tsn_vlan_added:1;
	s32 sra_idleslope_res;
	s32 srb_idleslope_res;



	bool est_enabled;

	unsigned long state;
	struct workqueue_struct *wq;
	struct work_struct service_task;
	int tx_irq;
	int rx_irq;

	u32 dma_ch_int_en[XJ3_MAX_DMA_CH];
#ifdef CONFIG_HOBOT_DMC_CLK
	struct hobot_dpm eth_dfs;
	u32 dfs_gmac_hash0_31;
	u32 dfs_gmac_hash32_63;
	u32 dfs_gmac_packet_filter;
#endif
};


enum tx_frame_status {
	tx_done = 0x0,
	tx_not_ls = 0x1,
	tx_err = 0x2,
	tx_dma_own = 0x4,
};


enum dma_irq_status {
	tx_hard_error = 0x1,
	tx_hard_error_bump_tc = 0x2,
	handle_rx = 0x4,
	handle_tx = 0x8,
};

enum rx_frame_status {
	good_frame = 0x0,
	discard_frame = 0x1,
	csum_none = 0x2,
	llc_snap = 0x4,
	dma_own = 0x8,
	rx_not_ls = 0x10,
};

#if 0
enum {
	STMMAC_SET_QMODE = 0x1,
	STMMAC_SET_CBS = 0x2,
	STMMAC_SET_EST = 0x3,
	STMMAC_SET_FPE = 0x4,
	STMMAC_GET_CBS = 0x5,
};

#endif


enum {
        STMMAC_GET_QMODE = 0x1,
        STMMAC_SET_QMODE = 0x2,
        STMMAC_GET_CBS = 0x3,
        STMMAC_SET_CBS = 0x4,
        STMMAC_GET_EST = 0x5,
        STMMAC_SET_EST = 0x6,
        STMMAC_GET_FPE = 0x7,
        STMMAC_SET_FPE = 0x8,
        STMMAC_GET_ECC = 0x9,
        STMMAC_SET_ECC = 0xa,
        STMMAC_GET_RXP = 0xb,
        STMMAC_SET_RXP = 0xc,
        STMMAC_GET_MCGR = 0xd,
        STMMAC_SET_MCGR = 0xe,
        STMMAC_GET_PPS = 0xf,
        STMMAC_SET_PPS = 0x10,
};




#define STMMAC_QMODE_DCB 0x0
#define STMMAC_QMODE_AVB 0x1

#define STMMAC_IOCTL_PPS_TRGTMODSEL_ONLY_INT	0x0
#define STMMAC_IOCTL_PPS_TRGTMODSEL_INT_ST	0x2
#define STMMAC_IOCTL_PPS_TRGTMODSEL_ONLY_ST	0x3

#define STMMAC_IOCTL_PPS_CMD_START_SINGLE_PULSE	0x1
#define STMMAC_IOCTL_PPS_CMD_START_PULSE_TRAIN	0x2
#define STMMAC_IOCTL_PPS_CMD_CANCEL_START	0x3
#define STMMAC_IOCTL_PPS_CMD_STOP_PULSE_TRAIN_TIME 0x4
#define STMMAC_IOCTL_PPS_CMD_STOP_PULSE_TARIN	0x5
#define STMMAC_IOCTL_PPS_CMD_CANCEL_STOP_PULSE_TRAIN 	0X6


struct xj3_ioctl_pps_cfg {
	__u32 cmd;
	__u32 index;
	__u32 enabled;
	__u32 ctrl_cmd;
	__u32 trgtmodsel;
	__u32 target_time[2];
	__u32 interval;
	__u32 width;
	__u32 freq;
};

struct xj3_qmode_cfg {
	__u32 cmd;
	__u32 queue_idx;
	__u32 queue_mode;
};

struct xj3_cbs_cfg {
	__u32 cmd;
	__u32 queue_idx;
	__u32 send_slope;
	__u32 idle_slope;
	__u32 high_credit;
	__u32 low_credit;
	__u32 percentage;
};

#define SIOCSTIOCTL	SIOCDEVPRIVATE





#define STMMAC_STAT(m)  \
	 { #m, sizeof_field(struct xj3_extra_stats, m),	\
	offsetof(struct xj3_priv, xstats.m)}

static const struct stmmac_stats stmmac_gstrings_stats[] = {
	/* Transmit errors */
	STMMAC_STAT(tx_underflow),
	STMMAC_STAT(tx_carrier),
	STMMAC_STAT(tx_losscarrier),
	STMMAC_STAT(vlan_tag),
	STMMAC_STAT(tx_deferred),
	STMMAC_STAT(tx_vlan),
	STMMAC_STAT(tx_jabber),
	STMMAC_STAT(tx_frame_flushed),
	STMMAC_STAT(tx_payload_error),
	STMMAC_STAT(tx_ip_header_error),
	/* Receive errors */
	STMMAC_STAT(rx_desc),
	STMMAC_STAT(sa_filter_fail),
	STMMAC_STAT(overflow_error),
	STMMAC_STAT(ipc_csum_error),
	STMMAC_STAT(rx_collision),
	STMMAC_STAT(rx_crc_errors),
	STMMAC_STAT(dribbling_bit),
	STMMAC_STAT(rx_length),
	STMMAC_STAT(rx_mii),
	STMMAC_STAT(rx_multicast),
	STMMAC_STAT(rx_gmac_overflow),
	STMMAC_STAT(rx_watchdog),
	STMMAC_STAT(da_rx_filter_fail),
	STMMAC_STAT(sa_rx_filter_fail),
	STMMAC_STAT(rx_missed_cntr),
	STMMAC_STAT(rx_overflow_cntr),
	STMMAC_STAT(rx_vlan),
	/* Tx/Rx IRQ error info */
	STMMAC_STAT(tx_undeflow_irq),
	STMMAC_STAT(tx_process_stopped_irq),
	STMMAC_STAT(tx_jabber_irq),
	STMMAC_STAT(rx_overflow_irq),
	STMMAC_STAT(rx_buf_unav_irq),
	STMMAC_STAT(rx_process_stopped_irq),
	STMMAC_STAT(rx_watchdog_irq),
	STMMAC_STAT(tx_early_irq),
	STMMAC_STAT(fatal_bus_error_irq),
	/* Tx/Rx IRQ Events */
	STMMAC_STAT(rx_early_irq),
	STMMAC_STAT(threshold),
	STMMAC_STAT(tx_pkt_n),
	STMMAC_STAT(rx_pkt_n),
	STMMAC_STAT(normal_irq_n),
	STMMAC_STAT(rx_normal_irq_n),
	STMMAC_STAT(napi_poll),
	STMMAC_STAT(tx_normal_irq_n),
	STMMAC_STAT(tx_clean),
	STMMAC_STAT(tx_set_ic_bit),
	STMMAC_STAT(irq_receive_pmt_irq_n),
	/* MMC info */
	STMMAC_STAT(mmc_tx_irq_n),
	STMMAC_STAT(mmc_rx_irq_n),
	STMMAC_STAT(mmc_rx_csum_offload_irq_n),
	/* EEE */
	STMMAC_STAT(irq_tx_path_in_lpi_mode_n),
	STMMAC_STAT(irq_tx_path_exit_lpi_mode_n),
	STMMAC_STAT(irq_rx_path_in_lpi_mode_n),
	STMMAC_STAT(irq_rx_path_exit_lpi_mode_n),
	STMMAC_STAT(phy_eee_wakeup_error_n),
	/* Extended RDES status */
	STMMAC_STAT(ip_hdr_err),
	STMMAC_STAT(ip_payload_err),
	STMMAC_STAT(ip_csum_bypassed),
	STMMAC_STAT(ipv4_pkt_rcvd),
	STMMAC_STAT(ipv6_pkt_rcvd),
	STMMAC_STAT(no_ptp_rx_msg_type_ext),
	STMMAC_STAT(ptp_rx_msg_type_sync),
	STMMAC_STAT(ptp_rx_msg_type_follow_up),
	STMMAC_STAT(ptp_rx_msg_type_delay_req),
	STMMAC_STAT(ptp_rx_msg_type_delay_resp),
	STMMAC_STAT(ptp_rx_msg_type_pdelay_req),
	STMMAC_STAT(ptp_rx_msg_type_pdelay_resp),
	STMMAC_STAT(ptp_rx_msg_type_pdelay_follow_up),
	STMMAC_STAT(ptp_rx_msg_type_announce),
	STMMAC_STAT(ptp_rx_msg_type_management),
	STMMAC_STAT(ptp_rx_msg_pkt_reserved_type),
	STMMAC_STAT(ptp_frame_type),
	STMMAC_STAT(ptp_ver),
	STMMAC_STAT(timestamp_dropped),
	STMMAC_STAT(av_pkt_rcvd),
	STMMAC_STAT(av_tagged_pkt_rcvd),
	STMMAC_STAT(vlan_tag_priority_val),
	STMMAC_STAT(l3_filter_match),
	STMMAC_STAT(l4_filter_match),
	STMMAC_STAT(l3_l4_filter_no_match),
	/* PCS */
	STMMAC_STAT(irq_pcs_ane_n),
	STMMAC_STAT(irq_pcs_link_n),
	STMMAC_STAT(irq_rgmii_n),
	/* DEBUG */
	STMMAC_STAT(mtl_tx_status_fifo_full),
	STMMAC_STAT(mtl_tx_fifo_not_empty),
	STMMAC_STAT(mmtl_fifo_ctrl),
	STMMAC_STAT(mtl_tx_fifo_read_ctrl_write),
	STMMAC_STAT(mtl_tx_fifo_read_ctrl_wait),
	STMMAC_STAT(mtl_tx_fifo_read_ctrl_read),
	STMMAC_STAT(mtl_tx_fifo_read_ctrl_idle),
	STMMAC_STAT(mac_tx_in_pause),
	STMMAC_STAT(mac_tx_frame_ctrl_xfer),
	STMMAC_STAT(mac_tx_frame_ctrl_idle),
	STMMAC_STAT(mac_tx_frame_ctrl_wait),
	STMMAC_STAT(mac_tx_frame_ctrl_pause),
	STMMAC_STAT(mac_gmii_tx_proto_engine),
	STMMAC_STAT(mtl_rx_fifo_fill_level_full),
	STMMAC_STAT(mtl_rx_fifo_fill_above_thresh),
	STMMAC_STAT(mtl_rx_fifo_fill_below_thresh),
	STMMAC_STAT(mtl_rx_fifo_fill_level_empty),
	STMMAC_STAT(mtl_rx_fifo_read_ctrl_flush),
	STMMAC_STAT(mtl_rx_fifo_read_ctrl_read_data),
	STMMAC_STAT(mtl_rx_fifo_read_ctrl_status),
	STMMAC_STAT(mtl_rx_fifo_read_ctrl_idle),
	STMMAC_STAT(mtl_rx_fifo_ctrl_active),
	STMMAC_STAT(mac_rx_frame_ctrl_fifo),
	STMMAC_STAT(mac_gmii_rx_proto_engine),
	/* TSO */
	STMMAC_STAT(tx_tso_frames),
	STMMAC_STAT(tx_tso_nfrags),
};


#define STMMAC_STATS_LEN ARRAY_SIZE(stmmac_gstrings_stats)


#define HOBOT_MMC_STAT(m) \
    { #m, sizeof_field(struct hobot_counters, m), \
    offsetof(struct xj3_priv, mmc.m)}

static const struct stmmac_stats hobot_mmc[] = {
    HOBOT_MMC_STAT(mmc_tx_octetcount_gb),
    HOBOT_MMC_STAT(mmc_tx_framecount_gb),
    HOBOT_MMC_STAT(mmc_tx_broadcastframe_g),
    HOBOT_MMC_STAT(mmc_tx_multicastframe_g),
    HOBOT_MMC_STAT(mmc_tx_64_octets_gb),
    HOBOT_MMC_STAT(mmc_tx_65_to_127_octets_gb),
    HOBOT_MMC_STAT(mmc_tx_128_to_255_octets_gb),
    HOBOT_MMC_STAT(mmc_tx_256_to_511_octets_gb),
    HOBOT_MMC_STAT(mmc_tx_512_to_1023_octets_gb),
    HOBOT_MMC_STAT(mmc_tx_1024_to_max_octets_gb),
    HOBOT_MMC_STAT(mmc_tx_unicast_gb),
    HOBOT_MMC_STAT(mmc_tx_multicast_gb),
    HOBOT_MMC_STAT(mmc_tx_broadcast_gb),
    HOBOT_MMC_STAT(mmc_tx_underflow_error),
    HOBOT_MMC_STAT(mmc_tx_singlecol_g),
    HOBOT_MMC_STAT(mmc_tx_multicol_g),
    HOBOT_MMC_STAT(mmc_tx_deferred),
    HOBOT_MMC_STAT(mmc_tx_latecol),
    HOBOT_MMC_STAT(mmc_tx_exesscol),
    HOBOT_MMC_STAT(mmc_tx_carrier_error),
    HOBOT_MMC_STAT(mmc_tx_octetcount_g),
    HOBOT_MMC_STAT(mmc_tx_framecount_g),
    HOBOT_MMC_STAT(mmc_tx_excessdef),
    HOBOT_MMC_STAT(mmc_tx_pause_frame),
    HOBOT_MMC_STAT(mmc_tx_vlan_frame_g),
    HOBOT_MMC_STAT(mmc_rx_framecount_gb),
    HOBOT_MMC_STAT(mmc_rx_octetcount_gb),
    HOBOT_MMC_STAT(mmc_rx_octetcount_g),
    HOBOT_MMC_STAT(mmc_rx_broadcastframe_g),
    HOBOT_MMC_STAT(mmc_rx_multicastframe_g),
    HOBOT_MMC_STAT(mmc_rx_crc_error),
    HOBOT_MMC_STAT(mmc_rx_align_error),
    HOBOT_MMC_STAT(mmc_rx_run_error),
    HOBOT_MMC_STAT(mmc_rx_jabber_error),
    HOBOT_MMC_STAT(mmc_rx_undersize_g),
    HOBOT_MMC_STAT(mmc_rx_oversize_g),
    HOBOT_MMC_STAT(mmc_rx_64_octets_gb),
    HOBOT_MMC_STAT(mmc_rx_65_to_127_octets_gb),
    HOBOT_MMC_STAT(mmc_rx_128_to_255_octets_gb),
    HOBOT_MMC_STAT(mmc_rx_256_to_511_octets_gb),
    HOBOT_MMC_STAT(mmc_rx_512_to_1023_octets_gb),
    HOBOT_MMC_STAT(mmc_rx_1024_to_max_octets_gb),
    HOBOT_MMC_STAT(mmc_rx_unicast_g),
    HOBOT_MMC_STAT(mmc_rx_length_error),
    HOBOT_MMC_STAT(mmc_rx_autofrangetype),
    HOBOT_MMC_STAT(mmc_rx_pause_frames),
    HOBOT_MMC_STAT(mmc_rx_fifo_overflow),
    HOBOT_MMC_STAT(mmc_rx_vlan_frames_gb),
    HOBOT_MMC_STAT(mmc_rx_watchdog_error),
    HOBOT_MMC_STAT(mmc_rx_ipc_intr_mask),
    HOBOT_MMC_STAT(mmc_rx_ipc_intr),
    HOBOT_MMC_STAT(mmc_rx_ipv4_gd),
    HOBOT_MMC_STAT(mmc_rx_ipv4_hderr),
    HOBOT_MMC_STAT(mmc_rx_ipv4_nopay),
    HOBOT_MMC_STAT(mmc_rx_ipv4_frag),
    HOBOT_MMC_STAT(mmc_rx_ipv4_udsbl),
    HOBOT_MMC_STAT(mmc_rx_ipv4_gd_octets),
    HOBOT_MMC_STAT(mmc_rx_ipv4_hderr_octets),
    HOBOT_MMC_STAT(mmc_rx_ipv4_nopay_octets),
    HOBOT_MMC_STAT(mmc_rx_ipv4_frag_octets),
    HOBOT_MMC_STAT(mmc_rx_ipv4_udsbl_octets),
    HOBOT_MMC_STAT(mmc_rx_ipv6_gd_octets),
    HOBOT_MMC_STAT(mmc_rx_ipv6_hderr_octets),
    HOBOT_MMC_STAT(mmc_rx_ipv6_nopay_octets),
    HOBOT_MMC_STAT(mmc_rx_ipv6_gd),
    HOBOT_MMC_STAT(mmc_rx_ipv6_hderr),
    HOBOT_MMC_STAT(mmc_rx_ipv6_nopay),
    HOBOT_MMC_STAT(mmc_rx_udp_gd),
    HOBOT_MMC_STAT(mmc_rx_udp_err),
    HOBOT_MMC_STAT(mmc_rx_tcp_gd),
    HOBOT_MMC_STAT(mmc_rx_tcp_err),
    HOBOT_MMC_STAT(mmc_rx_icmp_gd),
    HOBOT_MMC_STAT(mmc_rx_icmp_err),
    HOBOT_MMC_STAT(mmc_rx_udp_gd_octets),
    HOBOT_MMC_STAT(mmc_rx_udp_err_octets),
    HOBOT_MMC_STAT(mmc_rx_tcp_gd_octets),
    HOBOT_MMC_STAT(mmc_rx_tcp_err_octets),
    HOBOT_MMC_STAT(mmc_rx_icmp_gd_octets),
    HOBOT_MMC_STAT(mmc_rx_icmp_err_octets),
};

#define STMMAC_MMC_STATS_LEN ARRAY_SIZE(hobot_mmc)

enum hobot_state {
    HOBOT_DOWN,
	HOBOT_IN_XMIT,
};

void hobot_set_ethtool_ops(struct xj3_priv *priv);
void hobot_dma_rx_watchdog(struct xj3_priv *priv, u32 riwt, u32 number);
#endif
