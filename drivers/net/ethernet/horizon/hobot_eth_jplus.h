/*
 *Copyright (C) 2020 tao03.wang@horizon.ai.
 */

#ifndef __HOBOT_ETH_JPLUS_H_
#define __HOBOT_ETH_JPLUS_H_/*PRQA S 0603*/

#include <linux/stddef.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <asm/io.h>
#include <linux/clk.h>
#include <linux/ptp_clock_kernel.h>
#include <net/pkt_sched.h>
#include <linux/pinctrl/consumer.h>
#include <linux/net_tstamp.h>
#include <linux/phylink.h>
#include <linux/timer.h>
#include "hobot_reg.h"
#include "hobot_eth_mmc.h"
#if defined(CONFIG_HOBOT_FUSA_DIAG)
#include <linux/hobot_diag.h>
#endif


static inline u32 get_next_entry(u32 cur, s32 size)
{
	return (u32)((cur + 1U) & ((u32)size - 1U));
}


#define DRIVER_VERSION			"0.9"

static inline u32 ioreadl(const void __iomem *ioaddr, u32 offset)
{
        return readl(ioaddr + offset);/*PRQA S 0497*/
}
static inline void iowritel(u32 value, void __iomem *ioaddr, u32 offset)
{
        writel(value, ioaddr + offset);/*PRQA S 0497*/
}

/* Helpers to program the hobot eth fusa */
struct eth_stl_ops {
	/* MAC core initialization */
	s32 (*stl_init)(struct net_device *ndev);

	void (*stl_stop)(const struct net_device *ndev);

	void (*stl_resume)(const struct net_device *ndev);

	s32 (*stl_open)(const struct net_device *ndev);

	s32 (*stl_extension_ioctl)(const struct net_device *ndev, void __user *data);

	void (*stl_update_backup_regs)(struct net_device *ndev, u32 val, u32 reg);
};

struct rxq_routing {
	u32 reg_mask;
	u32 reg_shift;
};


struct extra_stats {
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

struct gmac_stats {
	s8 stat_name[ETH_GSTRING_LEN];
	s32 sizeof_stat;
	s32 stat_offset;
};


struct dma_features {
	u32 mbps_10_100;
	u32 mbps_1000;
	u32 half_duplex;
	u32 hash_filter;
	u32 multi_addr;
	u32 pcs;
	u32 sma_mdio;
	u32 pmt_remote_wake_up;
	u32 pmt_magic_frame;
	u32 rmon;
	/* IEEE 1588-2002 */
	u32 time_stamp;
	/* IEEE 1588-2008 */
	u32 atime_stamp;
	/* 802.3az - Energy-Efficient Ethernet (EEE) */
	u32 eee;
	u32 av;
	u32 tsoen;
	/* TX and RX csum */
	u32 tx_coe;
	u32 rx_coe;
	u32 rx_coe_type1;
	u32 rx_coe_type2;
	u32 rxfifo_over_2048;
	/* TX and RX number of channels */
	u32 number_rx_channel;
	u32 number_tx_channel;
	/* TX and RX number of queues */
	u32 number_rx_queues;
	u32 pps_out_num;

	u32 number_tx_queues;
	/* Alternate (enhanced) DESC mode */
	u32 enh_desc;
	/* TX and RX FIFO sizes */
	u32 tx_fifo_size;
	u32 rx_fifo_size;

	/*TSN feature*/
	u32 tsn;

	/*frame preemption*/
///	u32 tsn_frame_preemption;
	u32 fpesel;

	/*Enhancements to Scheduling Trafic*/
	//u32 tsn_enh_sched_traffic;
	u32 estsel;

	u32 estwid;
	u32 estdep;

	/*automotive safety package*/
	u32 asp;
	u32 frpsel;
	u32 frpbs;
	u32 frpes;
	u32 tbssel;
};


struct mac_resource {
	void __iomem *addr;
	void __iomem *addr_sub;
	const char *mac;
	s32 irq;
	s32 wol_irq;
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


struct dma_rx_queue {
	u32 queue_index;
	struct hobot_priv *priv_data;

	struct dma_ext_desc *dma_erx ____cacheline_aligned_in_smp;
	struct dma_desc *dma_rx;
	struct sk_buff **rx_skbuff;
	dma_addr_t *rx_skbuff_dma;

	u32 cur_rx;
	u32 dirty_rx;
	dma_addr_t dma_rx_phy;
	u32 rx_tail_addr;
	struct napi_struct napi;

	u32 rx_zeroc_thresh;
};

struct tx_info {
	dma_addr_t buf;
	bool map_as_page;
	unsigned len;
	bool last_segment;
	bool is_jumbo;

};
struct dma_tx_queue {
	u32 queue_index;
	struct hobot_priv *priv_data;
	struct dma_ext_desc *dma_etx ____cacheline_aligned_in_smp;
	struct dma_desc *dma_tx;

	struct sk_buff **tx_skbuff;
	struct tx_info *tx_skbuff_dma;
	u32 cur_tx;
	u32 dirty_tx;
	dma_addr_t dma_tx_phy;
	u32 tx_tail_addr;
	u32 mss;
};


struct rxq_cfg {
	u8 mode_to_use;
	u32 chan;
	u8 pkt_route;
	bool use_prio;
	u32 prio;
};

struct txq_cfg {
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

#define STMMAC_EST_GCL_MAX_ENTRIES		256
struct est_cfg {
	u32 cmd;
	u32 enabled;
	u32 estwid;
	u32 estdep;
	u32 btr_offset[2];
	u32 ctr[2];
	u32 ter;
	u32 gcl[STMMAC_EST_GCL_MAX_ENTRIES];
	u32 gcl_size;
};

struct fpe_cfg {
	u32 cmd;
	u32 enabled;
};

#define STMMAC_MAC_ADDR_MAX		7
struct unicast_cfg {
	u32 cmd;
	u32 enabled;
	u8 addr[STMMAC_MAC_ADDR_MAX][ETH_ALEN];
	u32 count;
};



struct dma_ctrl_cfg {
	s32 pbl;
	u32 txpbl;
	u32 rxpbl;

	bool pblx8;
	bool fixed_burst;
	bool mixed_burst;
	bool aal;

	u32 read_requests;
	u32 write_requests;
	u32 burst_map;
	bool en_lpi;

};

struct mdio_bus_data {
	int (*phy_reset)(void *priv);
	u32 phy_mask;
	s32 *irqs;
	s32 probed_phy_irq;
	s32 reset_gpio, active_low;
	u32 delays[3];
};

#define AXI_BLEN 7
struct axi_cfg {
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
struct plat_config_data {
	struct device_node *phy_node;
	struct device_node *mdio_node;
	s32 interface;

	u32 max_speed;
	s32 bus_id;
	s32 phy_addr;
	s32 clk_csr;
	u32 pmt;
	s32 maxmtu;
	struct axi_cfg *axi;



	struct mdio_bus_data *mdio_bus_data;

	struct rxq_cfg rx_queues_cfg[MTL_MAX_RX_QUEUES];
	struct txq_cfg tx_queues_cfg[MTL_MAX_TX_QUEUES];
	struct dma_ctrl_cfg *dma_cfg;

	struct clk *eth_apb_clk;
	struct clk *eth_bus_clk;
	struct clk *eth_mac_clk;
	struct clk *phy_ref_clk;
	struct clk *clk_ptp_ref;

	struct gpio_desc *phyreset;
	struct reset_control *reset;

	u32 rx_queues_to_use;
	u32 tx_queues_to_use;
	u8 rx_sched_algorithm;
	u8 tx_sched_algorithm;


	s32 rx_fifo_size;
	s32 tx_fifo_size;

	u32 rx_coe;
	u32 tx_coe;
	bool tso_en;

	s32 enh_desc;

	s32 force_no_rx_coe;
	s32 force_no_tx_coe;

	bool est_en;
	struct est_cfg est_cfg;
	bool fp_en;
	bool tbssel;
	bool unicast_en;

	u32 clk_ptp_rate;
	u32 speed_100M_max_rx_delay;
	u32 speed_100M_max_tx_delay;
	u32 speed_1G_max_rx_delay;
	u32 speed_1G_max_tx_delay;
	u32 mac_tx_delay;
	u32 mac_rx_delay;
	u32 cdc_delay;


	bool force_sf_dma_mode;
	bool force_thresh_dma_mode;
	bool en_tx_lpi_clockgating;
	s32 mac_port_sel_speed;
	struct device_node *phylink_node;
};



#define PPS_MAX 4
struct pps_cfg {
	bool available;
	struct timespec64 start;
	struct timespec64 period;

};

struct safety_irq_state {
	u32 mac_dpp_state;
	u32 mtl_ecc_state;
	u32 dma_ecc_state;
	u32 dma_safety_state;
	u32 mtl_safety_state;
};

#define CONF_REGS_SIZE 3U
struct conf_regs_st {
	u32 conf_reg;
	u32 value;
	const u32 mask;
};

struct hobot_priv {
	void __iomem *ioaddr;
	void __iomem *ioaddr_sub;

	struct net_device *ndev;
	struct device *device;
	struct mii_bus *mii;

	struct dma_rx_queue rx_queue[MTL_MAX_RX_QUEUES];
	struct dma_tx_queue tx_queue[MTL_MAX_TX_QUEUES];
	struct plat_config_data *plat;


	s32 hw_cap_support;
	struct dma_features dma_cap;
	struct hobot_counters mmc;
	u32 csr_val;

	spinlock_t lock;

	u32 rx_csum;

	u32 dma_buf_sz;

	s32 extend_desc;

	void __iomem *mmcaddr;
	bool tso;


	struct extra_stats xstats ____cacheline_aligned_in_smp;

	bool tx_path_in_lpi_mode;
	s32 hwts_rx_en;
	s32 hwts_tx_en;
	u32 msg_enable;
	u32 adv_ts;
	s32 speed;

	struct hwtstamp_config tstamp_config;
	u32 default_addend;
	struct ptp_clock *ptp_clock;
	struct ptp_clock_info ptp_clock_ops;
	raw_spinlock_t ptp_lock;
	u32 sub_second_inc;
	u32 systime_flags;
	struct pps_cfg pps[PPS_MAX];
//	struct ptp_extts_request extts;
	ktime_t suspend_ptp_time;
	ktime_t suspend_boot_time;

	u8 pcp_hi;
	u8 pcp_lo;

	s32 wol_irq;
	s32 irq_wake;
	s32 wolopts;

	bool est_enabled;

	unsigned long state;
	struct workqueue_struct *wq;
	struct work_struct service_task;

	/* Safety Features */
	u32 safety_enabled;
	u32 safety_err_inject;
	u32 safety_err_where;
	u32 ecc_err_correctable;
	struct phylink_config phylink_config;
	struct phylink *phylink;

	struct safety_irq_state irq_state;


	uint16_t safety_moduleId;
	struct eth_stl_ops *hobot_eth_stl_ops;
	struct conf_regs_st backup_reg_array[CONF_REGS_SIZE];
	struct timer_list txtimer;
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
	error_frame = 0x20,
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
        STMMAC_GET_SAFETY = 0x9,
        STMMAC_SET_SAFETY = 0xa,
        STMMAC_GET_RXP = 0xb,
        STMMAC_SET_RXP = 0xc,
        STMMAC_GET_MCGR = 0xd,
        STMMAC_SET_MCGR = 0xe,
        STMMAC_GET_PPS = 0xf,
        STMMAC_SET_PPS = 0x10,
        STMMAC_SET_FP  = 0x11,
        STMMAC_GET_FP  = 0x12,
        STMMAC_SET_UC  = 0x13,
        STMMAC_GET_UC  = 0x14,
};

/* MAC PMT bitmap */
enum power_event {
	global_unicast = 0x00000200,
	wake_up_rx_frame = 0x00000040,
	magic_frame = 0x00000020,
	wake_up_frame_en = 0x00000004,
	magic_pkt_en = 0x00000002,
	power_down = 0x00000001,
};



#define STMMAC_QMODE_DCB 0x0
#define STMMAC_QMODE_AVB 0x1


struct qmode_cfg {
	u32 cmd;
	u32 queue_idx;
	u32 queue_mode;
};

struct cbs_cfg {
	u32 cmd;
	u32 queue_idx;
	u32 send_slope;
	u32 idle_slope;
	u32 high_credit;
	u32 low_credit;
	u32 percentage;
};

#define SIOCSTIOCTL	SIOCDEVPRIVATE





#define GMAC_STAT(m)	\
	{ #m, sizeof_field(struct extra_stats, m),	\
	offsetof(struct hobot_priv, xstats.m)}

static const struct gmac_stats gmac_gstrings_stats[] = {
	/* Transmit errors */
	GMAC_STAT(tx_underflow),
	GMAC_STAT(tx_carrier),
	GMAC_STAT(tx_losscarrier),
	GMAC_STAT(vlan_tag),
	GMAC_STAT(tx_deferred),
	GMAC_STAT(tx_vlan),
	GMAC_STAT(tx_jabber),
	GMAC_STAT(tx_frame_flushed),
	GMAC_STAT(tx_payload_error),
	GMAC_STAT(tx_ip_header_error),
	/* Receive errors */
	GMAC_STAT(rx_desc),
	GMAC_STAT(sa_filter_fail),
	GMAC_STAT(overflow_error),
	GMAC_STAT(ipc_csum_error),
	GMAC_STAT(rx_collision),
	GMAC_STAT(rx_crc_errors),
	GMAC_STAT(dribbling_bit),
	GMAC_STAT(rx_length),
	GMAC_STAT(rx_mii),
	GMAC_STAT(rx_multicast),
	GMAC_STAT(rx_gmac_overflow),
	GMAC_STAT(rx_watchdog),
	GMAC_STAT(da_rx_filter_fail),
	GMAC_STAT(sa_rx_filter_fail),
	GMAC_STAT(rx_missed_cntr),
	GMAC_STAT(rx_overflow_cntr),
	GMAC_STAT(rx_vlan),
	/* Tx/Rx IRQ error info */
	GMAC_STAT(tx_undeflow_irq),
	GMAC_STAT(tx_process_stopped_irq),
	GMAC_STAT(tx_jabber_irq),
	GMAC_STAT(rx_overflow_irq),
	GMAC_STAT(rx_buf_unav_irq),
	GMAC_STAT(rx_process_stopped_irq),
	GMAC_STAT(rx_watchdog_irq),
	GMAC_STAT(tx_early_irq),
	GMAC_STAT(fatal_bus_error_irq),
	/* Tx/Rx IRQ Events */
	GMAC_STAT(rx_early_irq),
	GMAC_STAT(threshold),
	GMAC_STAT(tx_pkt_n),
	GMAC_STAT(rx_pkt_n),
	GMAC_STAT(normal_irq_n),
	GMAC_STAT(rx_normal_irq_n),
	GMAC_STAT(napi_poll),
	GMAC_STAT(tx_normal_irq_n),
	GMAC_STAT(tx_clean),
	GMAC_STAT(tx_set_ic_bit),
	GMAC_STAT(irq_receive_pmt_irq_n),
	/* MMC info */
	GMAC_STAT(mmc_tx_irq_n),
	GMAC_STAT(mmc_rx_irq_n),
	GMAC_STAT(mmc_rx_csum_offload_irq_n),
	/* EEE */
	GMAC_STAT(irq_tx_path_in_lpi_mode_n),
	GMAC_STAT(irq_tx_path_exit_lpi_mode_n),
	GMAC_STAT(irq_rx_path_in_lpi_mode_n),
	GMAC_STAT(irq_rx_path_exit_lpi_mode_n),
	GMAC_STAT(phy_eee_wakeup_error_n),
	/* Extended RDES status */
	GMAC_STAT(ip_hdr_err),
	GMAC_STAT(ip_payload_err),
	GMAC_STAT(ip_csum_bypassed),
	GMAC_STAT(ipv4_pkt_rcvd),
	GMAC_STAT(ipv6_pkt_rcvd),
	GMAC_STAT(no_ptp_rx_msg_type_ext),
	GMAC_STAT(ptp_rx_msg_type_sync),
	GMAC_STAT(ptp_rx_msg_type_follow_up),
	GMAC_STAT(ptp_rx_msg_type_delay_req),
	GMAC_STAT(ptp_rx_msg_type_delay_resp),
	GMAC_STAT(ptp_rx_msg_type_pdelay_req),
	GMAC_STAT(ptp_rx_msg_type_pdelay_resp),
	GMAC_STAT(ptp_rx_msg_type_pdelay_follow_up),
	GMAC_STAT(ptp_rx_msg_type_announce),
	GMAC_STAT(ptp_rx_msg_type_management),
	GMAC_STAT(ptp_rx_msg_pkt_reserved_type),
	GMAC_STAT(ptp_frame_type),
	GMAC_STAT(ptp_ver),
	GMAC_STAT(timestamp_dropped),
	GMAC_STAT(av_pkt_rcvd),
	GMAC_STAT(av_tagged_pkt_rcvd),
	GMAC_STAT(vlan_tag_priority_val),
	GMAC_STAT(l3_filter_match),
	GMAC_STAT(l4_filter_match),
	GMAC_STAT(l3_l4_filter_no_match),
	/* PCS */
	GMAC_STAT(irq_pcs_ane_n),
	GMAC_STAT(irq_pcs_link_n),
	GMAC_STAT(irq_rgmii_n),
	/* DEBUG */
	GMAC_STAT(mtl_tx_status_fifo_full),
	GMAC_STAT(mtl_tx_fifo_not_empty),
	GMAC_STAT(mmtl_fifo_ctrl),
	GMAC_STAT(mtl_tx_fifo_read_ctrl_write),
	GMAC_STAT(mtl_tx_fifo_read_ctrl_wait),
	GMAC_STAT(mtl_tx_fifo_read_ctrl_read),
	GMAC_STAT(mtl_tx_fifo_read_ctrl_idle),
	GMAC_STAT(mac_tx_in_pause),
	GMAC_STAT(mac_tx_frame_ctrl_xfer),
	GMAC_STAT(mac_tx_frame_ctrl_idle),
	GMAC_STAT(mac_tx_frame_ctrl_wait),
	GMAC_STAT(mac_tx_frame_ctrl_pause),
	GMAC_STAT(mac_gmii_tx_proto_engine),
	GMAC_STAT(mtl_rx_fifo_fill_level_full),
	GMAC_STAT(mtl_rx_fifo_fill_above_thresh),
	GMAC_STAT(mtl_rx_fifo_fill_below_thresh),
	GMAC_STAT(mtl_rx_fifo_fill_level_empty),
	GMAC_STAT(mtl_rx_fifo_read_ctrl_flush),
	GMAC_STAT(mtl_rx_fifo_read_ctrl_read_data),
	GMAC_STAT(mtl_rx_fifo_read_ctrl_status),
	GMAC_STAT(mtl_rx_fifo_read_ctrl_idle),
	GMAC_STAT(mtl_rx_fifo_ctrl_active),
	GMAC_STAT(mac_rx_frame_ctrl_fifo),
	GMAC_STAT(mac_gmii_rx_proto_engine),
	/* TSO */
	GMAC_STAT(tx_tso_frames),
	GMAC_STAT(tx_tso_nfrags),
};


#define GMAC_STATS_LEN ARRAY_SIZE(gmac_gstrings_stats)

#define HOBOT_MMC_STAT(m) \
    { #m, sizeof_field(struct hobot_counters, m), \
    offsetof(struct hobot_priv, mmc.m)}

static const struct gmac_stats hobot_mmc[] = {
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



/* Memory Errors */
#define STMMAC_IOCTL_ECC_ERR_TSO		0
#define STMMAC_IOCTL_ECC_ERR_EST		1
/* Parity Errors in FSM */
#define STMMAC_IOCTL_ECC_ERR_FSM_REVMII		2
#define STMMAC_IOCTL_ECC_ERR_FSM_RX125		3
#define STMMAC_IOCTL_ECC_ERR_FSM_TX125		4
#define STMMAC_IOCTL_ECC_ERR_FSM_PTP		5
#define STMMAC_IOCTL_ECC_ERR_FSM_APP		6
#define STMMAC_IOCTL_ECC_ERR_FSM_CSR		7
#define STMMAC_IOCTL_ECC_ERR_FSM_RX		8
#define STMMAC_IOCTL_ECC_ERR_FSM_TX		9
/* Timeout Errors in FSM */
#define STMMAC_IOCTL_ECC_ERR_FSM_TREVMII	10
#define STMMAC_IOCTL_ECC_ERR_FSM_TRX125		11
#define STMMAC_IOCTL_ECC_ERR_FSM_TTX125		12
#define STMMAC_IOCTL_ECC_ERR_FSM_TPTP		13
#define STMMAC_IOCTL_ECC_ERR_FSM_TAPP		14
#define STMMAC_IOCTL_ECC_ERR_FSM_TCSR		15
#define STMMAC_IOCTL_ECC_ERR_FSM_TRX		16
#define STMMAC_IOCTL_ECC_ERR_FSM_TTX		17
/* Parity Errors in DPP */
#define STMMAC_IOCTL_ECC_ERR_DPP_CSR		18
#define STMMAC_IOCTL_ECC_ERR_DPP_AXI		19
#define STMMAC_IOCTL_ECC_ERR_DPP_RX		20
#define STMMAC_IOCTL_ECC_ERR_DPP_TX		21
#define STMMAC_IOCTL_ECC_ERR_DPP_DMATSO		22
#define STMMAC_IOCTL_ECC_ERR_DPP_DMADTX		23
#define STMMAC_IOCTL_ECC_ERR_DPP_MTLRX		24
#define STMMAC_IOCTL_ECC_ERR_DPP_MTLTX		25
#define STMMAC_IOCTL_ECC_ERR_DPP_MTL		26
#define STMMAC_IOCTL_ECC_ERR_DPP_INTERFACE	27

struct safety_cfg {
	u32 cmd;
	u32 supported;
	u32 enabled;
	u32 err_inject_supported;
	u32 err_inject;
	u32 err_where;
	u32 err_correctable;
};



enum stmmac_state {
	STMMAC_DOWN,
	STMMAC_RESET_REQUESTED,
	STMMAC_RESETING,
	STMMAC_SERVICE_SCHED,
};




struct hwtstamp_ctr_config{
	u32 ptp_v2;
	u32 tstamp_all;
	u32 ptp_over_ipv4_udp;
	u32 ptp_over_ipv6_udp;
	u32 ptp_over_ethernet;
	u32 snap_type_sel;
	u32 ts_master_en;
	u32 ts_event_en;
};



#if (defined CONFIG_JPLUS_ETH_FUSA) || (defined CONFIG_JPLUS_ETH_FUSA_MODULE)
void setup_eth_stl_ops(struct hobot_priv *priv);
#else
static inline void setup_eth_stl_ops(struct hobot_priv *priv)
{
	priv->hobot_eth_stl_ops = NULL;
}
#endif

void tsn_config_cbs(const struct hobot_priv *priv, u32 send_slope,
			u32 idle_slope, u32 high_credit, u32 low_credit,
			u32 queue);
s32 est_write_reg(const struct hobot_priv *priv, u32 reg, u32 val, bool is_gcla);

int tc_init(struct hobot_priv *priv);

int tc_setup_cbs(struct hobot_priv *priv, struct tc_cbs_qopt_offload *qopt);
s32 tc_setup_taprio(struct hobot_priv *priv, struct tc_taprio_qopt_offload *qopt);

#endif
