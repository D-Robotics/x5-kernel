/*
 * Horizon Robotics
 *
 *  Copyright (C) 2020 Horizon Robotics Inc.
 *  All rights reserved.
 *
 * This is stl function for eth driver
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#define pr_fmt(fmt) "[eth_stl]:" fmt
#include "hobot_eth_jplus.h"
#include <linux/of.h>






#define MIN_SND_MS 1
#define MAX_SND_MS 512
#define FCHM_ERR_CODE 0xffff
#define MAX_REG_BIT 32
#define LOOPBACK_CLK_RATE 125000000

static const struct conf_regs_st backup_reg_array[CONF_REGS_SIZE] = {
	{ GMAC_CONFIG,			0,		0xFFFFFFFFU},
	{ MTL_OPERATION_MODE,	0,		0x8367U},
	{ DMA_SYS_BUS_MODE,		0,		0xFCFFU},
};


static const char *dwmac5_mac_errors[MAX_REG_BIT] = {
	"ATPES: Application Transmit Interface Parity Check Error",
	"TPES: TSO Data Path Parity Check Error",
	"RDPES: Read Descriptor Parity Check Error",
	"MPES: MTL Data Path Parity Check Error",
	"MTSPES: MTL TX Status Data Path Parity Check Error",
	"ARPES: Application Receive Interface Data Path Parity Check Error",
	"CWPES: CSR Write Data Path Parity Check Error",
	"ASRPES: AXI Slave Read Data Path Parity Check Error",
	"TTES: TX FSM Timeout Error",
	"RTES: RX FSM Timeout Error",
	"CTES: CSR FSM Timeout Error",
	"ATES: APP FSM Timeout Error",
	"PTES: PTP FSM Timeout Error",
	"T125ES: TX125 FSM Timeout Error",
	"R125ES: RX125 FSM Timeout Error",
	"RVCTES: REV MDC FSM Timeout Error",
	"MSTTES: Master Read/Write Timeout Error",
	"SLVTES: Slave Read/Write Timeout Error",
	"Unknown Error", /* 18 */
	"Unknown Error", /* 19 */
	"Unknown Error", /* 20 */
	"Unknown Error", /* 21 */
	"Unknown Error", /* 22 */
	"Unknown Error", /* 23 */
	"FSMPES: FSM State Parity Error",
	"Unknown Error", /* 25 */
	"Unknown Error", /* 26 */
	"Unknown Error", /* 27 */
	"Unknown Error", /* 28 */
	"Unknown Error", /* 29 */
	"Unknown Error", /* 30 */
	"Unknown Error", /* 31 */
};
static const char *dwmac5_mtl_errors[MAX_REG_BIT] = {
	"TXCES: MTL TX Memory Error",
	"TXAMS: MTL TX Memory Address Mismatch Error",
	"TXUES: MTL TX Memory Error",
	"Unknown Error", /* 3 */
	"RXCES: MTL RX Memory Error",
	"RXAMS: MTL RX Memory Address Mismatch Error",
	"RXUES: MTL RX Memory Error",
	"Unknown Error", /* 7 */
	"ECES: MTL EST Memory Error",
	"EAMS: MTL EST Memory Address Mismatch Error",
	"EUES: MTL EST Memory Error",
	"Unknown Error", /* 11 */
	"RPCES: MTL RX Parser Memory Error",
	"RPAMS: MTL RX Parser Memory Address Mismatch Error",
	"RPUES: MTL RX Parser Memory Error",
	"Unknown Error", /* 15 */
	"Unknown Error", /* 16 */
	"Unknown Error", /* 17 */
	"Unknown Error", /* 18 */
	"Unknown Error", /* 19 */
	"Unknown Error", /* 20 */
	"Unknown Error", /* 21 */
	"Unknown Error", /* 22 */
	"Unknown Error", /* 23 */
	"Unknown Error", /* 24 */
	"Unknown Error", /* 25 */
	"Unknown Error", /* 26 */
	"Unknown Error", /* 27 */
	"Unknown Error", /* 28 */
	"Unknown Error", /* 29 */
	"Unknown Error", /* 30 */
	"Unknown Error", /* 31 */
};
static const char *dwmac5_dma_errors[MAX_REG_BIT] = {
	"TCES: DMA TSO Memory Error",
	"TAMS: DMA TSO Memory Address Mismatch Error",
	"TUES: DMA TSO Memory Error",
	"Unknown Error", /* 3 */
	"Unknown Error", /* 4 */
	"Unknown Error", /* 5 */
	"Unknown Error", /* 6 */
	"Unknown Error", /* 7 */
	"Unknown Error", /* 8 */
	"Unknown Error", /* 9 */
	"Unknown Error", /* 10 */
	"Unknown Error", /* 11 */
	"Unknown Error", /* 12 */
	"Unknown Error", /* 13 */
	"Unknown Error", /* 14 */
	"Unknown Error", /* 15 */
	"Unknown Error", /* 16 */
	"Unknown Error", /* 17 */
	"Unknown Error", /* 18 */
	"Unknown Error", /* 19 */
	"Unknown Error", /* 20 */
	"Unknown Error", /* 21 */
	"Unknown Error", /* 22 */
	"Unknown Error", /* 23 */
	"Unknown Error", /* 24 */
	"Unknown Error", /* 25 */
	"Unknown Error", /* 26 */
	"Unknown Error", /* 27 */
	"Unknown Error", /* 28 */
	"Unknown Error", /* 29 */
	"Unknown Error", /* 30 */
	"Unknown Error", /* 31 */
};

/**
 *  prepare_tx_desc_stl
 *  @p : dma descriptor
 *  @is_fs : is first descriptor or not
 *  @len : length
 *  @csum_flags : checksum flag
 *  @tx_own : tx owner
 *  @ls : is last descriptor or not
 *  @tot_pkt_len : packet length
 *  Description : prepare tx stl descriptor
 *  Return: NA
 */
/* code review E1: do not need to return value */
static void prepare_tx_desc_stl(struct dma_desc *p, s32 is_fs, s32 len, bool csum_flags, s32 tx_own, bool ls, u32 tot_pkt_len)
{
	u32 tdes3 = le32_to_cpu(p->des3);

	p->des2 |= cpu_to_le32((u32)len & TDES2_BUFFER1_SIZE_MASK);/*PRQA S 0636, 4501, 0478, 1882*/

	tdes3 |= tot_pkt_len & (u32)TDES3_PACKET_SIZE_MASK;/*PRQA S 0636, 4501, 0478, 1882*/

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



/**
 *  eth_netdev_loopback_transmit
 *  @skb : the socket buffer
 *  @dev : device pointer
 *  Description : transmit function, only for stl loopback test
 *  It programs the chain or the ring and supports oversized frames
 *  and SG feature.
 *  Return: 0 on success, otherwise error.
 */
static netdev_tx_t eth_netdev_loopback_transmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct hobot_priv *priv;
	u32 nopaged_len;
	u32 nfrags;
	u32 entry;
	u32 first_entry;
	u32 queue;
	struct dma_tx_queue *tx_q;
	struct dma_desc *desc, *first;
	dma_addr_t dma_addr;
	struct timespec64 now;
	bool csum_insert;
	bool last_segment;
	if (unlikely((NULL == ndev) || (NULL == skb))) {
		(void)pr_err("%s, para is null\n", __func__);
		return NETDEV_TX_BUSY;
	}
	priv = (struct hobot_priv *)netdev_priv(ndev);
	nopaged_len = skb_headlen(skb);
	nfrags = (u32)(skb_shinfo(skb)->nr_frags);/*PRQA S 3305*/
	queue = skb_get_queue_mapping(skb);
	pr_debug("nr_frags=%d, skb len:%d, skb is gso:%d, priv->tso:%d, skb_shinfo->gso_type:0x%x\n",/*PRQA S 0685, 1294, 3305*/
		nfrags, skb->len, skb_is_gso(skb), priv->tso, skb_shinfo(skb)->gso_type);
	tx_q = &priv->tx_queue[queue];


	entry = tx_q->cur_tx;
	first_entry = entry;

	pr_debug("%s,tx queue:%d, entry:%d\n", __func__, queue, entry);/*PRQA S 0685, 1294*/
	csum_insert = (skb->ip_summed == (u8)CHECKSUM_PARTIAL);

	if (0 != priv->extend_desc)
		desc = (struct dma_desc *)(tx_q->dma_etx + entry);
	else
		desc = tx_q->dma_tx + entry;

	first = desc;

	tx_q->tx_skbuff[entry] = skb;
	entry = get_next_entry(entry, DMA_TX_SIZE);
	tx_q->cur_tx = entry;


	ndev->stats.tx_bytes += skb->len;/*PRQA S 2812*/


	desc->des2 |= cpu_to_le32(TDES2_INTERRUPT_ON_COMPLETION);


	skb_tx_timestamp(skb);


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

	if ((skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) && priv->hwts_tx_en) {/*PRQA S 1861, 3305*/
		skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;/*PRQA S 1861, 3305*/
		first->des2 |= cpu_to_le32(TDES2_TIMESTAMP_ENABLE);
		ktime_get_real_ts64(&now);
	}

	prepare_tx_desc_stl(first, 1, (s32)nopaged_len, csum_insert, 1, last_segment, skb->len);
	dma_wmb();


#ifdef HOBOT_ETH_LOG_DEBUG
	print_pkt(skb->data, skb->len);
#endif


	iowritel(tx_q->tx_tail_addr, priv->ioaddr, DMA_CHAN_TX_END_ADDR(queue));
	netif_trans_update(ndev);

	return NETDEV_TX_OK;


dma_map_err:
	dev_kfree_skb(skb);
	priv->ndev->stats.tx_dropped++;
	return NETDEV_TX_OK;
}


/**
 *  eth_diag_send_event
 *  @module_id : module id
 *  @event_id : event id
 *  @event_prio : event priority
 *  @err_code : error code 
 *  Description : eth driver send diagnosis event
 *  Return: NA
 */
static void eth_diag_send_event(uint16_t module_id, uint16_t event_id, uint8_t event_prio, uint16_t err_code)
{
	struct diag_event event;
	event.module_id = module_id;
	event.event_id = event_id;
	event.event_prio = event_prio;
	event.event_sta = (uint8_t)DiagEventStaFail;
	event.fchm_err_code = err_code;
	event.env_len = 0U;
	if (diagnose_send_event(&event) < 0) {
		pr_err("%s:send event error\n", __func__);
	}
	return;
}


/**
 *	safety_log_error
 *	@ndev : net device struct pointer
 *	@value : mac state value
 *	@event_id : event id
 *	@errors_str : errors string pointer
 *	@error : err_info struct pointer
 *	Description : eth driver safety log print, and send event to diag system
 *	Return: NA
 */
static void safety_log_error(const struct net_device *ndev, u32 value,
				uint16_t event_id, const char *const* errors_str, const struct err_info *error)
{
	unsigned long loc, mask;
	struct diag_event event;
	struct hobot_priv *priv = (struct hobot_priv *)netdev_priv(ndev);
	mask = value;

	for_each_set_bit(loc, &mask, MAX_REG_BIT) {
		netdev_err(ndev, "'%s'\n", errors_str[loc]);
		event.event_id = event_id;
		if ( ((priv->irq_state.mtl_safety_state & (MCSIS | MEUIS)) != 0U) ||
		     ((priv->irq_state.dma_safety_state & (MCSIS | MSUIS | DEUIS)) != 0U) ) {
			event.event_prio = (uint8_t)DiagMsgPrioMid;
		} else {
			event.event_prio = (uint8_t)DiagMsgPrioLow;
		}
		event.module_id = (uint16_t)priv->safety_moduleId;
		event.event_sta = (uint8_t)DiagEventStaFail;
		event.fchm_err_code = (u16)error->errcode;
		event.env_len = 0;
		if ( diagnose_send_event(&event) < 0 ) {
			netdev_err(ndev, "send event error\n");
		}
	}
}


/**
 *	handle_safety_err
 *	@priv : hobot private struct pointer
 *	@error : err_info struct pointer
 *	Description : eth driver safety error handle function
 *	Return: NA
 */
static void handle_safety_err(const struct hobot_priv *priv, const struct err_info *error)
{
	uint32_t mac_state;
	pr_debug("[%s] begin\n", __func__);/*PRQA S 1294, 0685*/

	if (priv->irq_state.mac_dpp_state != 0U) {
		mac_state = (u32)(ATPES | TPES | RDPES | MPES | MTSPES | ARPES | CWPES | ASRPES | FSMPES) &
					priv->irq_state.mac_dpp_state;
		if ( mac_state != 0U ) {
			safety_log_error(priv->ndev, mac_state, (uint16_t)EventIdParityCheckErr, dwmac5_mac_errors, error);
		}

		mac_state = (u32)(TTES | RTES | CTES | ATES | PTES | T125ES | R125ES | RVCTES) &
					priv->irq_state.mac_dpp_state;
		if ( mac_state != 0U ) {
			safety_log_error(priv->ndev, mac_state, (uint16_t)EventIdFsmTimeout, dwmac5_mac_errors, error);
		}

		mac_state = (u32)(MSTTES | SLVTES) & priv->irq_state.mac_dpp_state;
		if ( mac_state != 0U ) {
			safety_log_error(priv->ndev, mac_state, (uint16_t)EventIdRWTimeout, dwmac5_mac_errors, error);
		}
	}

	if (priv->irq_state.mtl_ecc_state != 0U) {
		safety_log_error(priv->ndev, priv->irq_state.mtl_ecc_state,
				(uint16_t)EventIdEccErr, dwmac5_mtl_errors, error);
	}

	if (priv->irq_state.dma_ecc_state != 0U) {
		safety_log_error(priv->ndev, priv->irq_state.dma_ecc_state,
				(uint16_t)EventIdEccErr, dwmac5_dma_errors, error);
	}

	return;
}

static void reset_subtask(const struct hobot_priv *priv, const struct err_info *error)
{

	if (test_bit(STMMAC_DOWN, &priv->state)) {
		return;
	}
	handle_safety_err(priv, error);

	return;
#if 0
	rtnl_lock();
	netif_trans_update(priv->ndev);
	while (test_and_set_bit(STMMAC_RESETTING, &priv->state))
		usleep_range(1000, 2000);

	set_bit(STMMAC_DOWN, &priv->state);
	dev_close(priv->ndev);

	smp_mb__before_atomic();
	clear_bit(STMMAC_DOWN, &priv->state);
	clear_bit(STMMAC_RESETTING, &priv->state);
	dev_open(priv->ndev);
	rtnl_unlock();
#endif
}


/**
 *	verify_backup_regs_value
 *	@priv : hobot private struct pointer
 *	Description : verify backup register value
 *	Return: 0 on success, otherwise error
 */
static int32_t verify_backup_regs_value(const struct hobot_priv *priv)
{
	uint32_t i;
	uint32_t reg_val;
	const void __iomem *ioaddr = priv->ioaddr;

	for (i = 0; i < CONF_REGS_SIZE; i++) {
		reg_val = ioreadl(ioaddr, priv->backup_reg_array[i].conf_reg) & priv->backup_reg_array[i].mask;
		if (reg_val != priv->backup_reg_array[i].value) {
			return -1;
		}
	}
	return 0;
}

/**
 *	eth_stl_update_backup_regs
 *	@ndev : net device struct pointer
 *  @val: data value
 *  @reg: register value
 *	Description : verify backup register value
 *	Return: 0 on success, otherwise error
 */
/* code review E1: do not need to return value */
static void eth_stl_update_backup_regs(struct net_device *ndev, u32 val, u32 reg)
{
	uint32_t i;
	struct hobot_priv *priv = (struct hobot_priv *)netdev_priv(ndev);

	for (i = 0; i < CONF_REGS_SIZE; i++) {
		if (reg == priv->backup_reg_array[i].conf_reg) {
			priv->backup_reg_array[i].value = val & priv->backup_reg_array[i].mask;
			return;
		}
	}
}



 /**
 *  safety_enable
 *  @priv : hobot private pointer
 *  @enable : enable flag
 *  Description : It is used for configuring the safety features
 *  Return: NA
 */
static void safety_enable(const struct hobot_priv *priv, uint32_t enable)
{
	void __iomem *ioaddr = priv->ioaddr;
	u32 clk_rate;
	uint64_t val_mtl_ecc, val_mtl_ecc_irq, val_dma_ecc_irq,
				val_mac_fsm, val_mtl_dpp, val_fsm_timer;
	uint32_t asp = priv->dma_cap.asp;

	if ( asp == 0U ) {
		return;
	}

	netdev_info(priv->ndev, "Enabling safety features: type=0x%x\n", asp);
	val_mtl_ecc = ioreadl(ioaddr, MTL_ECC_CONTROL);
	val_mtl_ecc_irq = ioreadl(ioaddr, MTL_ECC_INT_ENABLE);
	val_dma_ecc_irq = ioreadl(ioaddr, DMA_ECC_INT_ENABLE);
	val_mtl_dpp = ioreadl(ioaddr, MTL_DPP_CONTROL);
	val_mac_fsm = ioreadl(ioaddr, MAC_FSM_CONTROL);
	val_fsm_timer = ioreadl(ioaddr, MAC_FSM_ACT_TIMER);
	if (enable != 0U) {
		/* 1. Enable Safety Features */
		val_mtl_ecc |= TSOEE; /* TSO ECC */
		val_mtl_ecc |= MESTEE; /* MTL EST ECC */
		val_mtl_ecc |= MRXEE; /* MTL RX FIFO ECC */
		val_mtl_ecc |= MTXEE; /* MTL TX FIFO ECC */

		/* 2. Enable MTL Safety Interrupts */
		val_mtl_ecc_irq |= ECEIE; /* EST Memory Correctable Error */
		val_mtl_ecc_irq |= RXCEIE; /* RX Memory Correctable Error */
		val_mtl_ecc_irq |= TXCEIE; /* TX Memory Correctable Error */

		/* 3. Enable DMA Safety Interrupts */
		val_dma_ecc_irq |= TCEIE; /* TSO Memory Correctable Error */

		/* 4. Enable Parity and Timeout for FSM */
		val_mac_fsm |= PRTYEN; /* FSM Parity Feature */
		val_mac_fsm |= TMOUTEN; /* FSM Timeout Feature */

		clk_rate = (uint32_t)clk_get_rate(priv->plat->eth_bus_clk);
		val_fsm_timer |= NTMRMD(NTMRMD_64MS); /* FSM timeout value */
		val_fsm_timer |= TMR(clk_rate/1000000);/* number of CSR clocks per us*/ /*PRQA S 1891*/

		/* 5. Enable Data Parity Protection */
		val_mtl_dpp |= EDPP;
	} else {
		/* 1. Disable Safety Features */
		val_mtl_ecc &= ~TSOEE; /* TSO ECC */
		val_mtl_ecc &= ~MRXPEE; /* MTL RX Parser ECC */
		val_mtl_ecc &= ~MESTEE; /* MTL EST ECC */
		val_mtl_ecc &= ~MRXEE; /* MTL RX FIFO ECC */
		val_mtl_ecc &= ~MTXEE; /* MTL TX FIFO ECC */

		/* 2. Disable MTL Safety Interrupts */
		val_mtl_ecc_irq &= ~RPCEIE; /* RX Parser Memory Correctable Error */
		val_mtl_ecc_irq &= ~ECEIE; /* EST Memory Correctable Error */
		val_mtl_ecc_irq &= ~RXCEIE; /* RX Memory Correctable Error */
		val_mtl_ecc_irq &= ~TXCEIE; /* TX Memory Correctable Error */

		/* 3. Disable DMA Safety Interrupts */
		val_dma_ecc_irq &= ~TCEIE; /* TSO Memory Correctable Error */

		/* 4. Disable Data Parity Protection */
		val_mtl_dpp &= ~EDPP;
		val_mtl_dpp &= ~EPSI;

		/* 5. disable Parity and Timeout for FSM */
		val_mac_fsm &= ~PRTYEN; /* FSM Parity Feature */
		val_mac_fsm &= ~TMOUTEN; /* FSM Timeout Feature */

		val_fsm_timer |= NTMRMD(0U);
		val_fsm_timer |= TMR(0U);
	}

	iowritel((u32)val_mtl_ecc, ioaddr, MTL_ECC_CONTROL);
	iowritel((u32)val_mtl_ecc_irq, ioaddr, MTL_ECC_INT_ENABLE);
	iowritel((u32)val_dma_ecc_irq, ioaddr, DMA_ECC_INT_ENABLE);
	iowritel((u32)val_mtl_dpp, ioaddr, MTL_DPP_CONTROL);
	iowritel((u32)val_mac_fsm, ioaddr, MAC_FSM_CONTROL);
	iowritel((u32)val_fsm_timer, ioaddr, MAC_FSM_ACT_TIMER);
}


 /**
 *  safety_config
 *  @priv : hobot private pointer
 *  Description : It is used for configuring the safety features
 *  Return: NA
 */
static void safety_config(struct hobot_priv *priv)
{
	safety_enable(priv, (uint32_t)true);
	priv->safety_enabled = (uint32_t)true;
	priv->safety_err_inject = (uint32_t)false;
	priv->safety_err_where = 0;
	priv->ecc_err_correctable = (uint32_t)false;
}


/**
 * get_safety_irq_status
 * @priv: hobot private struct pointer
 * Description: get safety irq status
 * Return: 0 on success, otherwise error
 */
static uint32_t get_safety_irq_status(struct hobot_priv *priv)
{
	void __iomem *ioaddr = priv->ioaddr;
	uint32_t safety_irq;
	u32 value;

	if (priv->dma_cap.asp == 0U) {
		return 0U;
	}

	priv->irq_state.mtl_safety_state =
		ioreadl(ioaddr, MTL_SAFETY_INT_STATUS);
	priv->irq_state.dma_safety_state =
		ioreadl(ioaddr, DMA_SAFETY_INT_STATUS);

	safety_irq = priv->irq_state.mtl_safety_state |
		     priv->irq_state.dma_safety_state;

	/*clear irq status flags*/
	value = ioreadl(ioaddr, MAC_DPP_FSM_INT_STATUS);
	iowritel(value, ioaddr, MAC_DPP_FSM_INT_STATUS);
	priv->irq_state.mac_dpp_state = value;

	value = ioreadl(ioaddr, MTL_ECC_INT_STATUS);
	iowritel(value, ioaddr, MTL_ECC_INT_STATUS);
	priv->irq_state.mtl_ecc_state = value;

	value = ioreadl(ioaddr, DMA_ECC_INT_STATUS);
	iowritel(value, ioaddr, DMA_ECC_INT_STATUS);
	priv->irq_state.dma_ecc_state = value;

	pr_debug(/*PRQA S 0685, 1294*/
				"mtl_safety=0x%x, dma_safety=0x%x, mac_dpp=0x%x, mtl_ecc=0x%x, dma_ecc=0x%x\n",
				priv->irq_state.mtl_safety_state,
				priv->irq_state.dma_safety_state,
				priv->irq_state.mac_dpp_state,
				priv->irq_state.mtl_ecc_state,
				priv->irq_state.dma_ecc_state);

	return safety_irq;
}


/**
 * irq_invalid_check
 * @priv: hobot private struct pointer
 * Description: check irq valid or not
 * Return: 0 on success, otherwise error
 */
static int32_t irq_invalid_check(const struct hobot_priv *priv)
{
	u32 mtl_dpp_irq_en, mac_fsm_irq_en, mtl_ecc_irq_en, dma_ecc_irq_en;

	mtl_dpp_irq_en = ioreadl(priv->ioaddr, MTL_DPP_CONTROL);
	mac_fsm_irq_en = ioreadl(priv->ioaddr, MAC_FSM_CONTROL);
	mtl_ecc_irq_en = ioreadl(priv->ioaddr, MTL_ECC_INT_ENABLE);
	dma_ecc_irq_en = ioreadl(priv->ioaddr, DMA_ECC_INT_ENABLE);
	if ( (priv->irq_state.mac_dpp_state != 0U) &&
	    (!(((mtl_dpp_irq_en & EDPP) != 0U) && ((mac_fsm_irq_en & PRTYEN) != 0U) &&
	       ((mac_fsm_irq_en & TMOUTEN) != 0U)))) {
		netdev_err(priv->ndev, "mac dpp irq invalid\n");
		return -1;
	}
	if ((priv->irq_state.mtl_ecc_state != 0U) &&
	    (!(((mtl_ecc_irq_en & ECEIE) != 0U) && ((mtl_ecc_irq_en & RXCEIE) != 0U) &&
	        ((mtl_ecc_irq_en & TXCEIE) != 0U)) )
		) {
		netdev_err(priv->ndev, "mtl ecc irq invalid\n");
		return -1;
	}

	if ( (priv->irq_state.dma_ecc_state != 0U) && (!((dma_ecc_irq_en & TCEIE) != 0U)) ) {
		netdev_err(priv->ndev, "dma ecc irq invalid\n");
		return -1;
	}

	return 0;
}


/**
 * disable_dma_irq
 * @priv: hobot private struct pointer
 * @chan: channel id
 * Description: disable dma irq
 * Return:NA
 */
static void disable_dma_irq(const struct hobot_priv *priv, u32 chan)
{
	iowritel(0, priv->ioaddr, DMA_CHAN_INTR_ENA(chan));
}

/**
 * enable_dma_irq
 * @priv: hobot private struct pointer
 * @chan: channel id
 * Description: enable dma irq
 * Return:NA
 */
static inline void enable_dma_irq(const struct hobot_priv *priv, u32 chan)
{
	iowritel(DMA_CHAN_INTR_DEFAULT_MASK,
	       priv->ioaddr, DMA_CHAN_INTR_ENA(chan));
}
static uint32_t dma_interrupt(void __iomem *ioaddr, struct extra_stats *x, u32 chan)
{
	uint32_t ret = 0;
	u32 intr_status = ioreadl(ioaddr, DMA_CHAN_STATUS(chan));
	pr_debug("dma_interrupt chan%u intr_status=0x%x\n", chan,/*PRQA S 0685, 1294*/
			intr_status);

	if ((intr_status & DMA_CHAN_STATUS_AIS) != 0U) {
		if ((intr_status & DMA_CHAN_STATUS_TPS) != 0U) {
			x->tx_process_stopped_irq++;
			ret = (uint32_t)tx_hard_error;
		}
		if ((intr_status & DMA_CHAN_STATUS_FBE) != 0U) {
			x->fatal_bus_error_irq++;
			ret = (uint32_t)tx_hard_error;
		}
	}

	/* loopback test, only focus rx status */
	x->normal_irq_n++;
	if ((intr_status & DMA_CHAN_STATUS_RI) != 0U) {
		x->rx_normal_irq_n++;
		ret |= (uint32_t)handle_rx;
	}
	if ((intr_status & DMA_CHAN_STATUS_TI) != 0U) {
		x->tx_normal_irq_n++;
		ret |= (uint32_t)handle_tx;
	}
	/* clear interrupt status */
	iowritel((intr_status & DMA_CHAN_STATUS_MASK), ioaddr, DMA_CHAN_STATUS(chan));
	return ret;
}


/**
 * get_rx_status
 * @x: extra_stats struct pointer
 * @p: dma_desc struct pointer
 * Description: get rx frmae status
 * Return: >=0 frame status
 */
static enum rx_frame_status get_rx_status(struct extra_stats *x, const struct dma_desc *p)
{
	unsigned int rdes2 = le32_to_cpu(p->des2);
	unsigned int rdes3 = le32_to_cpu(p->des3);

	enum rx_frame_status ret = good_frame;

	pr_debug("rdes2:0x%x, rdes3:0x%x\n", rdes2, rdes3);/*PRQA S 0685, 1294*/
	if ((rdes3 & RDES3_OWN) != 0U) {
		return dma_own;
	}

	if ((rdes3 & RDES3_LAST_DESCRIPTOR) == 0U) {
		return discard_frame;
	}
	if ((rdes3 & RDES3_ERROR_SUMMARY) != 0U) {
		ret = discard_frame;
	}


	if ((rdes2 & RDES2_SA_FILTER_FAIL) != 0U) {
		x->sa_rx_filter_fail++;
		ret = discard_frame;
	}

	return ret;
}




/*arp packet for loopback test*/
const uint8_t lpback_test_data[ETH_ZLEN] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x08, 0x06, 0x00, 0x01,
	0x08, 0x00, 0x06, 0x04, 0x00, 0x01, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0xc0, 0xa8, 0x01, 0x02,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xa8, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint32_t loopback_rx_packet(struct hobot_priv *priv, uint32_t limit, uint32_t queue)
{
	struct dma_rx_queue *rx_q = &priv->rx_queue[queue];
	uint32_t entry = rx_q->cur_rx;

	uint32_t next_entry;
	uint32_t count = 0;
	struct net_device *ndev = priv->ndev;
	enum rx_frame_status status;
	struct dma_desc *p, *np;

	while (count < limit) {
		p = &rx_q->dma_rx[entry];

		status = get_rx_status(&priv->xstats, p);
		if (status == dma_own) {
			break;
		}

		rx_q->cur_rx = get_next_entry(rx_q->cur_rx, DMA_RX_SIZE);
		next_entry = rx_q->cur_rx;

		pr_debug("%s, cur_rx:0x%x\n", __func__, rx_q->cur_rx);/*PRQA S 0685, 1294*/
		np = &rx_q->dma_rx[next_entry];
		prefetch(np);

		if ((status == discard_frame) || (status == error_frame)) {
			priv->ndev->stats.rx_errors++;
			dma_unmap_single(priv->device, rx_q->rx_skbuff_dma[entry], priv->dma_buf_sz, 
							DMA_FROM_DEVICE);
			dev_kfree_skb_any(rx_q->rx_skbuff[entry]);
			rx_q->rx_skbuff[entry] = NULL;
		} else {
			struct sk_buff *skb;
			uint32_t frame_len;

			frame_len = (le32_to_cpu(p->des3) & RDES3_PACKET_SIZE_MASK);/*PRQA S 0636, 4501, 0478, 4461, 1882*/
			if (frame_len > priv->dma_buf_sz) {
				netdev_err(priv->ndev, "len %d larger than size (%d)\n", frame_len, priv->dma_buf_sz);
				priv->ndev->stats.rx_length_errors++;
				priv->ndev->stats.rx_errors++;
				break;
			}

			if ((ndev->features & NETIF_F_RXFCS) == 0U) {
				frame_len -= (uint32_t)ETH_FCS_LEN;
			}

			skb = rx_q->rx_skbuff[entry];
			if (skb == NULL) {
				netdev_err(priv->ndev, "inconsistent Rx chain\n");
				priv->ndev->stats.rx_dropped++;
				break;
			}

			prefetch(skb->data - NET_IP_ALIGN);
			skb_put(skb, frame_len);
			dma_unmap_single(priv->device, rx_q->rx_skbuff_dma[entry], priv->dma_buf_sz, DMA_FROM_DEVICE);

			pr_debug("%s len = %d byte, buf addr: 0x%p\n", __func__, skb->len, skb->data);/*PRQA S 0685, 1294*/
			print_hex_dump_bytes("", DUMP_PREFIX_OFFSET, skb->data, skb->len);/*PRQA S 0685, 1294*/

			/*compare tx data with rx data*/
			if ( (frame_len == ETH_ZLEN) && (0 == memcmp(skb->data, lpback_test_data, ETH_ZLEN)) ) {
				count++;
			}

			dev_kfree_skb_any(rx_q->rx_skbuff[entry]);
			rx_q->rx_skbuff[entry] = NULL;
		}

		entry = next_entry;
	}


	return count;
}

/**
 * safety_loopback_enable
 * @priv: hobot private struct pointer
 * @enable: enable flag
 * Description: enable eth loopback function
 * called by safety_loopback_selftest
 * Return: NA
 */
static void safety_loopback_enable(struct hobot_priv *priv, uint8_t enable)
{
	uint64_t value;
	void __iomem *ioaddr = priv->ioaddr;

	if (enable == 1U) {
		value = ioreadl(ioaddr, GMAC_CONFIG);
		value &= ~(GMAC_CONFIG_PS | GMAC_CONFIG_FES | GMAC_CONFIG_DM);
		value |= GMAC_CONFIG_LM | GMAC_CONFIG_DM | GMAC_CONFIG_TE | GMAC_CONFIG_RE;
		iowritel((u32)value, ioaddr, GMAC_CONFIG);

		clk_set_rate(priv->plat->eth_loopback_clk, LOOPBACK_CLK_RATE);
		clk_prepare_enable(priv->plat->eth_loopback_clk);
	} else {
		value = ioreadl(ioaddr, GMAC_CONFIG);
		value &= ~(GMAC_CONFIG_LM | GMAC_CONFIG_TE | GMAC_CONFIG_RE);
		iowritel((u32)value, ioaddr, GMAC_CONFIG);
		clk_disable_unprepare(priv->plat->eth_loopback_clk);
	}
}


static s32 get_loopback_tx_status(const struct dma_desc *p)
{
	u32 tdes3;
	s32 ret = (s32)tx_done;

	tdes3 = le32_to_cpu(p->des3);

	if (0U != (tdes3 & (u32)TDES3_OWN))
		return (s32)tx_dma_own;

	if (0U == (tdes3 & TDES3_LAST_DESCRIPTOR)) {
		return (s32)tx_not_ls;
	}
	if (0U != (tdes3 & TDES3_ERROR_SUMMARY)) {
		ret = (s32)tx_err;
	}

	return ret;
}


/**
 * tx_res_clean - to manage the transmission completion
 * @priv: driver private structure
 * @queue: TX queue index
 * Description: it reclaims the transmit resources after transmission completes.
 */
static void tx_loopback_res_clean(struct hobot_priv *priv, u32 queue)
{
	struct dma_tx_queue *tx_q = &priv->tx_queue[queue];
	u32 entry;

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
		status = get_loopback_tx_status(p);
		if (0U != ((u32)status & (u32)tx_dma_own)) {
			pr_debug("%s,queue:%d, status: 0x%x, and hw own tx dma\n", __func__, queue, status);/*PRQA S 0685, 1294*/
			break;
		}

		dma_rmb();

		if (0U == ((u32)status & (u32)tx_not_ls)) {
			if (0U != ((u32)status & (u32)tx_err)) {
				priv->ndev->stats.tx_errors++;
			} else {
				priv->ndev->stats.tx_packets++;
			}
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
			dev_consume_skb_any(skb);
			tx_q->tx_skbuff[entry] = NULL;
		}

		p->des2 = 0;
		p->des3 = 0;
		entry = get_next_entry(entry, DMA_TX_SIZE);
	}

	tx_q->dirty_tx = entry;

}

#define TX_DELAY_US 1000

/**
 * safety_loopback_selftest
 * @priv: hobot private struct pointer
 * Description: eth driver stl loopback for test.
 * Return: 0: success ; otherwise error
 */
static int32_t safety_loopback_selftest(struct hobot_priv *priv)
{
	struct sk_buff *skb;
	u32 status;
	u32 chan;
	u32 rx_cnt;
	u32 pkg_cnt = 0;
	netdev_tx_t tx_state;


	if (priv == NULL || priv->ndev == NULL || priv->ndev->netdev_ops == NULL) {
		return -1;
	}
	rx_cnt = priv->plat->rx_queues_to_use;
	skb = dev_alloc_skb(ETH_ZLEN);
	if (skb == NULL) {
		netdev_err(priv->ndev, "out of memory\n");
		return -ENOMEM;
	}
	skb_put_data(skb, lpback_test_data, ETH_ZLEN);
	skb->dev = priv->ndev;
	skb->len = ETH_ZLEN;


	for (chan = 0; chan < rx_cnt; chan++) {
		disable_dma_irq(priv, chan);
	}

	safety_loopback_enable(priv, 1U);

	/* call loopback transmit function */
	tx_state = eth_netdev_loopback_transmit(skb, skb->dev);
	if (NETDEV_TX_BUSY == tx_state) {
		netdev_err(priv->ndev, "loopback teset tx skb fail\n");
		dev_kfree_skb_any(skb);
		goto tx_err;
	}
	udelay(TX_DELAY_US);/*PRQA S 2880*/

	for (chan = 0; chan < rx_cnt; chan++) {
		status = dma_interrupt(priv->ioaddr, &priv->xstats, chan);
		if ((status & (u32)handle_rx) != 0U)  {
			pkg_cnt += loopback_rx_packet(priv, 1U, chan);
		}
		if ((status & (u32)handle_tx) != 0U)  {
			tx_loopback_res_clean(priv, chan);
		}
	}

tx_err:
	for (chan = 0; chan < rx_cnt; chan++) {
		enable_dma_irq(priv, chan);
	}

	safety_loopback_enable(priv, 0U);

	pr_debug("loopback test pkg_cnt=%u\n", pkg_cnt);/*PRQA S 0685, 1294*/
	if (pkg_cnt != 0U) {
		return 0;
	}
	return -1;
}



/**
 * ecc_control_update
 * @cfg: hobot safety_cfg struct pointer
 * @ioaddr: ioaddr pointer
 * Description: update ecc control register
 * Return: NA
 */
static inline void ecc_control_update(const struct safety_cfg *cfg, void __iomem *ioaddr)
{
	uint64_t value;

	switch (cfg->err_where) {
		case STMMAC_IOCTL_ECC_ERR_TSO:
			value = ioreadl(ioaddr, MTL_DBG_CTL);
			if (cfg->err_inject != 0U) {
				value |= (uint32_t)EIEE;
				if (cfg->err_correctable == 0U) {
					value &= ~(EIEC_2BIT | EIEC_3BIT);
					value |= EIEC_2BIT;
				}
			} else {
				value &= ~(EIEE | EIEC_2BIT | EIEC_3BIT);
			}
			iowritel(value, ioaddr, MTL_DBG_CTL);/*PRQA S 4461*/
			break;
		case STMMAC_IOCTL_ECC_ERR_EST:
			value = ioreadl(ioaddr, MTL_EST_GCL_CONTROL);
			if (cfg->err_inject != 0U) {
				value |= MTL_EST_ESTEIEE;
				if (cfg->err_correctable == 0U) {
					value &= ~(MTL_EST_ESTEIEC_2BIT | MTL_EST_ESTEIEC_3BIT);
					value |= MTL_EST_ESTEIEC_2BIT;
				}
			} else {
				value &= ~(MTL_EST_ESTEIEE | MTL_EST_ESTEIEC_2BIT | MTL_EST_ESTEIEC_3BIT);
			}
			iowritel(value, ioaddr, MTL_EST_GCL_CONTROL);/*PRQA S 4461*/
			break;
		default:
			pr_err("%s, err_where %u not support\n", __func__, cfg->err_where);
			break;
	}
	return;

}


/**
 * fsm_control_update
 * @cfg: hobot safety_cfg struct pointer
 * @ioaddr: ioaddr pointer
 * Description: update fsm control register
 * Return: NA
 */
static inline void fsm_control_update(const struct safety_cfg *cfg, void __iomem *ioaddr)
{
	uint64_t value;
	uint64_t bit;

	switch (cfg->err_where) {
		case STMMAC_IOCTL_ECC_ERR_FSM_REVMII:
			bit = RVCPEIN;
			break;
		case STMMAC_IOCTL_ECC_ERR_FSM_RX125:
			bit = R125PEIN;
			break;
		case STMMAC_IOCTL_ECC_ERR_FSM_TX125:
			bit = T125PEIN;
			break;
		case STMMAC_IOCTL_ECC_ERR_FSM_PTP:
			bit = PPEIN;
			break;
		case STMMAC_IOCTL_ECC_ERR_FSM_APP:
			bit = APEIN;
			break;
		case STMMAC_IOCTL_ECC_ERR_FSM_CSR:
			bit = CPEIN;
			break;
		case STMMAC_IOCTL_ECC_ERR_FSM_RX:
			bit = RPEIN;
			break;
		case STMMAC_IOCTL_ECC_ERR_FSM_TX:
			bit = TPEIN;
			break;
		case STMMAC_IOCTL_ECC_ERR_FSM_TREVMII:
			bit = RVCTEIN;
			break;
		case STMMAC_IOCTL_ECC_ERR_FSM_TRX125:
			bit = R125TEIN;
			break;
		case STMMAC_IOCTL_ECC_ERR_FSM_TTX125:
			bit = T125TEIN;
			break;
		case STMMAC_IOCTL_ECC_ERR_FSM_TPTP:
			bit = PTEIN;
			break;
		case STMMAC_IOCTL_ECC_ERR_FSM_TAPP:
			bit = ATEIN;
			break;
		case STMMAC_IOCTL_ECC_ERR_FSM_TCSR:
			bit = CTEIN;
			break;
		case STMMAC_IOCTL_ECC_ERR_FSM_TRX:
			bit = RTEIN;
			break;
		case STMMAC_IOCTL_ECC_ERR_FSM_TTX:
			bit = TTEIN;
			break;
		default:
			pr_err("%s, err_where %u not support\n", __func__, cfg->err_where);
			return;
		}

	value = ioreadl(ioaddr, MAC_FSM_CONTROL);
	if (cfg->err_inject != 0U) {
		value |= (bit);
	} else {
		value &= ~(bit);
	}
	iowritel(value, ioaddr, MAC_FSM_CONTROL);/*PRQA S 4461*/
}

/**
 * dpp_control_update
 * @cfg: hobot safety_cfg struct pointer
 * @ioaddr: ioaddr pointer
 * Description: update dpp control register
 * Return: NA
 */
static inline void dpp_control_update(const struct safety_cfg *cfg, void __iomem *ioaddr)
{
	uint64_t value;
	uint64_t bit;

	/* get bit by err_where */
	switch (cfg->err_where) {
	case STMMAC_IOCTL_ECC_ERR_DPP_CSR:
		bit = IPECW;
		break;
	case STMMAC_IOCTL_ECC_ERR_DPP_AXI:
		bit = IPEASW;
		break;
	case STMMAC_IOCTL_ECC_ERR_DPP_RX:
		bit = IPERD;
		break;
	case STMMAC_IOCTL_ECC_ERR_DPP_TX:
		bit = IPETD;
		break;
	case STMMAC_IOCTL_ECC_ERR_DPP_DMATSO:
		bit = IPETSO;
		break;
	case STMMAC_IOCTL_ECC_ERR_DPP_DMADTX:
		bit = IPEDDC;
		break;
	case STMMAC_IOCTL_ECC_ERR_DPP_MTLRX:
		bit = IPEMRF;
		break;
	case STMMAC_IOCTL_ECC_ERR_DPP_MTLTX:
		bit = IPEMTS;
		break;
	case STMMAC_IOCTL_ECC_ERR_DPP_MTL:
		bit = IPEMC;
		break;
	case STMMAC_IOCTL_ECC_ERR_DPP_INTERFACE:
		bit = IPEID;
		break;
	default:
		pr_err("%s, err_where %u not support\n", __func__, cfg->err_where);
		return;
	}

	value = ioreadl(ioaddr, MTL_DPP_CONTROL);
	if (cfg->err_inject != 0U) {
		value |= (bit);
	} else {
		value &= ~(bit);
	}
	iowritel((u32)value, ioaddr, MTL_DPP_CONTROL);
}



/**
 * safety_set_config
 * @priv: hobot private struct pointer
 * @data: data pointer from user
 * Description: set safety config 
 * called by eth_stl_extension_ioctl
 * Return: 0 on success, otherwise error
 */
static int32_t safety_set_config(struct hobot_priv *priv, const void __user *data)
{
	struct safety_cfg cfg;
	void __iomem *ioaddr = priv->ioaddr;
	int32_t ret = 0;

	if (copy_from_user(&cfg, data, sizeof(cfg)) != 0U) {
		netdev_err(priv->ndev, "copy from user failed\n");
		return -EFAULT;
	}
	if (priv->dma_cap.asp == 0U) {
		return -EOPNOTSUPP;
	}
	pr_debug(/*PRQA S 0685, 1294*/
		"enabled=%d, err_inject=%d, err_where=%d, err_correctable=%d\n",
		cfg.enabled, cfg.err_inject, cfg.err_where,
		cfg.err_correctable);

	if ((cfg.enabled == 0U) && (cfg.err_inject != 0U)) {
		netdev_err(priv->ndev, "can not inject error when safety disabled\n");
		return ret;
	}

	priv->safety_err_inject = cfg.err_inject;
	priv->safety_err_where = cfg.err_where;
	priv->ecc_err_correctable = cfg.err_correctable;
	priv->safety_enabled = cfg.enabled;
	safety_enable(priv, priv->safety_enabled);

	if (cfg.err_where <= (uint32_t)STMMAC_IOCTL_ECC_ERR_EST) {
		ecc_control_update(&cfg, ioaddr);
	} else if (cfg.err_where <= (uint32_t)STMMAC_IOCTL_ECC_ERR_FSM_TTX) {
		fsm_control_update(&cfg, ioaddr);
	} else if (cfg.err_where <= (uint32_t)STMMAC_IOCTL_ECC_ERR_DPP_INTERFACE) {
		dpp_control_update(&cfg, ioaddr);
	} else {
		ret = -EINVAL;
		netdev_err(priv->ndev, "ioctl err_where not found\n");
	}
	return ret;
}


/**
 * safety_get_config
 * @priv: hobot private struct pointer
 * @data: data pointer to user
 * Description: get safety config 
 * called by eth_stl_extension_ioctl
 * Return: 0 on success, otherwise error
 */
static int32_t safety_get_config(const struct hobot_priv *priv, void __user *data)
{
	struct safety_cfg cfg;

	(void)memset((void *)&cfg, 0, sizeof(cfg));
	cfg.supported = (priv->dma_cap.asp > 0U) ? 1U : 0U;
	cfg.enabled = priv->safety_enabled;
	cfg.err_inject_supported = true;
	cfg.err_inject = priv->safety_err_inject;
	cfg.err_where = priv->safety_err_where;
	cfg.err_correctable = priv->ecc_err_correctable;

	if (copy_to_user(data, &cfg, sizeof(cfg)) != 0U) {
		netdev_err(priv->ndev, "copy to user failed\n");
		return -EFAULT;
	}
	return 0;
}

/**
 * eth_stl_extension_ioctl - stl ioctl interface for err inject
 * @priv: The net_device struct
 * @data: user data ptr include cmd and config
 */
static int32_t eth_stl_extension_ioctl(const struct net_device *ndev, void __user *data)
{
	uint32_t cmd;
	int32_t ret;
	struct hobot_priv *priv;


	if (NULL == ndev) {
		pr_err("%s, net dev ptr is null\n", __func__);
		return -EINVAL;
	}
	priv = (struct hobot_priv *)netdev_priv(ndev);
	if (copy_from_user(&cmd, data, sizeof(cmd)) != 0U) {
		netdev_err(priv->ndev, "copy from user failed\n");
		return -EFAULT;
	}
	switch (cmd) {
	case STMMAC_SET_SAFETY:
		ret = safety_set_config(priv, data);
		break;
	case STMMAC_GET_SAFETY:
		ret = safety_get_config(priv, data);
		break;
	default:
		pr_err("%s, cmd %u not support\n", __func__, cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}


/**
 * eth_fusa_polling_event
 * @data: hobot private pointer
 * @error: err_info struct pointer
 * Description: polling fusa stl event 
 * Return: 0 on success, otherwise error
 */
static int32_t eth_fusa_polling_event(void *data, struct err_info error)
{
	struct hobot_priv *priv;
	if (NULL == data) {
		pr_err("%s, data ptr is null\n", __func__);
		return -EINVAL;
	}
	priv = (struct hobot_priv *)data;
	if (!netif_carrier_ok(priv->ndev)) {
		return 0;
	}
	if (verify_backup_regs_value(priv) < 0) {
		netdev_err(priv->ndev, "reg backup calibration error\n");
		eth_diag_send_event(priv->safety_moduleId, (uint16_t)EventIdIntervalRBackErr,
					(uint8_t)DiagMsgPrioLow, FCHM_ERR_CODE);
	}
	return 0;
}

/**
 * eth_fusa_checkerr_event
 * @data: hobot private pointer
 * @error: err_info struct pointer
 * Description: handle checkerr event
 * Return: 0 on success, otherwise error
 */
static int32_t eth_fusa_checkerr_event(void *data, struct err_info error)
{
	struct hobot_priv *priv;
	uint32_t ret;
	if (NULL == data) {
		pr_err("%s, data ptr is null\n", __func__);
		return  -EINVAL;
	}
	priv = (struct hobot_priv *)data;

	ret = get_safety_irq_status(priv);
	if (ret != 0U) {
		if (irq_invalid_check(priv) != 0) {
			netdev_err(priv->ndev, "invalid safety irq\n");
			eth_diag_send_event(priv->safety_moduleId, (uint16_t)EventIdIrqInvalid,
								(uint8_t)DiagMsgPrioLow, FCHM_ERR_CODE);
		}
		reset_subtask(priv, &error);
	} else {
		eth_diag_send_event(priv->safety_moduleId, (uint16_t)EventIdIrqInvalid,
							(uint8_t)DiagMsgPrioLow, FCHM_ERR_CODE);
	}

	return 0;
}


static int32_t eth_fusa_void_event(void *data, struct err_info error)
{
	pr_debug("%s,%d\n", __func__, __LINE__);/*PRQA S 0685, 1294*/
	return 0;
}

static struct diag_register_info eth_diag_info = {
	0,
	{
		{ (uint8_t)EventIdPoll, MIN_SND_MS, MAX_SND_MS, eth_fusa_polling_event, NULL },
		{ (uint8_t)EventIdCheckErr, MIN_SND_MS, MAX_SND_MS, eth_fusa_checkerr_event, NULL },
		{ (uint8_t)EventIdErrInject, MIN_SND_MS, MAX_SND_MS, eth_fusa_void_event, NULL },
		{ (uint8_t)EventIdLoopback, MIN_SND_MS, MAX_SND_MS, eth_fusa_void_event, NULL },
		{ (uint8_t)EventIdIrqInvalid, MIN_SND_MS, MAX_SND_MS, eth_fusa_void_event, NULL },
		{ (uint8_t)EventIdIntervalRBackErr, MIN_SND_MS, MAX_SND_MS, eth_fusa_void_event, NULL },
		{ (uint8_t)EventIdParityCheckErr, MIN_SND_MS, MAX_SND_MS, eth_fusa_void_event, NULL },
		{ (uint8_t)EventIdEccErr,  MIN_SND_MS, MAX_SND_MS, eth_fusa_void_event, NULL },
		{ (uint8_t)EventIdFsmTimeout, MIN_SND_MS, MAX_SND_MS, eth_fusa_void_event, NULL },
		{ (uint8_t)EventIdRWTimeout,  MIN_SND_MS, MAX_SND_MS, eth_fusa_void_event, NULL },
	},
	(u16)EventIdEthCnt,
};


/**
 * eth_diag_register
 * @priv: hobot private struct pointer
 * Description: eth driver register diagnostic function 
 * with diag system.
 * Return: 0: success ; otherwise error
 */
static int32_t eth_diag_register(struct hobot_priv *priv)
{
	uint32_t i;
	for (i = 0; i < eth_diag_info.event_cnt; i++) {
		eth_diag_info.event_handle[i].data = (void *)priv;
	}
	eth_diag_info.module_id = (uint8_t)priv->safety_moduleId;
	return diagnose_register(&eth_diag_info);
}


/**
 * eth_diag_unregister
 * @priv: hobot private struct pointer
 * Description: eth driver unregister diagnostic function 
 * with diag system.
 * Return: 0: success ; otherwise error
 */
static int32_t eth_diag_unregister(const struct hobot_priv *priv)
{
	return diagnose_unregister((uint8_t)priv->safety_moduleId);
}



/**
 * eth_stl_open
 * @ndev: net device struct pointer
 * Description: eth stl function open
 * called by eth open
 * Return: 0: success ; otherwise error
 */
static int32_t eth_stl_open(const struct net_device *ndev)
{
	int32_t ret;
	struct hobot_priv *priv;
	if (NULL == ndev) {
		pr_err("%s, net dev ptr is null\n", __func__);
		return -EINVAL;
	}
	priv = (struct hobot_priv *)netdev_priv(ndev);
	safety_config(priv);

	ret = eth_diag_register(priv);
	if (ret < 0) {
		netdev_err(priv->ndev, "eth diag event register err, ret=0x%x\n", ret);
		goto err_open;
	}
#ifdef HOBOT_ETH_LOOPBACK_EN
	if (safety_loopback_selftest(priv) != 0) {
		eth_diag_send_event(priv->safety_moduleId, (uint16_t)EventIdLoopback,
					(uint8_t)DiagMsgPrioHigh, FCHM_ERR_CODE);
		netdev_err(priv->ndev, "safety loopback selftest fail\n");
		ret = -EINVAL;
	}
#endif
	netdev_info(priv->ndev, "%s init ok\n", __func__);
	return 0;

err_open:
	safety_enable(priv, (uint32_t)false);
	priv->safety_enabled = (uint32_t)false;
	return ret;
}


static void eth_stl_resume(const struct net_device *ndev)
{
	struct hobot_priv *priv;
	if (NULL == ndev) {
		pr_err("%s, net dev ptr is null\n", __func__);
		return;
	}
	priv = (struct hobot_priv *)netdev_priv(ndev);
	/* open safety config */
	safety_enable(priv, (uint32_t)true);
	priv->safety_enabled = (uint32_t)true;
}


/**
 * eth_get_moduleId_by_ifindex
 * @ndev: net device struct pointer
 * Description: get eth stl module ID
 * called by eth_stl_init
 * Return: 0 on success, otherwise error
 */
static inline int32_t eth_get_moduleId_by_ifindex(const struct net_device *ndev)
{
	uint16_t moduleId = 0;
	int32_t ret = 0;
	struct hobot_priv *priv = (struct hobot_priv *)netdev_priv(ndev);

	switch (priv->plat->module_index) {
	case 0:
		moduleId = (uint16_t)ModuleDiag_eth0;
		break;
	case 1:
		moduleId = (uint16_t)ModuleDiag_eth1;
		break;
	default:
		netdev_err(ndev, "net device ifindex error:%d\n", priv->plat->module_index);
		ret = -1;
		break;
	}
	priv->safety_moduleId = moduleId;
	return ret;
}



/**
 * backup_reg_init
 * @ndev: net device struct pointer
 * Description: init backup register
 * called by eth_stl_init
 * Return: NA
 */
static void backup_reg_init(struct net_device *ndev)
{
	struct hobot_priv *priv = (struct hobot_priv *)netdev_priv(ndev);
	(void)memcpy((void*)priv->backup_reg_array, (const void *)backup_reg_array, sizeof(backup_reg_array));
	return;
}

/**
 * eth_stl_stop - stl init interface
 * @priv: The net_device struct
 * @return: 0 on success, otherwise error
 */
static int32_t eth_stl_init(struct net_device *ndev)
{
	int32_t ret;
	struct hobot_priv *priv;
	if (NULL == ndev) {
		pr_err("%s, net dev ptr is null\n", __func__);
		return -EINVAL;
	}
	priv = (struct hobot_priv *)netdev_priv(ndev);
	if (NULL == priv->plat) {
		pr_err("%s, net dev ptr is null\n", __func__);
		return -EINVAL;
	}
	ret = eth_get_moduleId_by_ifindex(ndev);
	if (ret < 0) {
		netdev_err(ndev, "eth module index error\n");
		return ret;
	}

	priv->plat->eth_loopback_clk = devm_clk_get(priv->device, "eth_loopback_clk");
	if (IS_ERR_OR_NULL(priv->plat->eth_loopback_clk)) {
		netdev_err(ndev, "Can't get dts eth_loopback_clk\n");
		priv->plat->eth_loopback_clk = NULL;
		return -EPERM;
	}
	backup_reg_init(ndev);
	return 0;
}

/**
 * eth_stl_stop - stl stop interface
 * @priv: The net_device struct
 * @return: NA
 */
static void eth_stl_stop(const struct net_device *ndev)
{
	int32_t ret;
	struct hobot_priv *priv;
	if (NULL == ndev) {
		pr_err("%s, net dev ptr is null\n", __func__);
		return;
	}
	priv = (struct hobot_priv *)netdev_priv(ndev);
	/* close safety config */
	safety_enable(priv, (uint32_t)false);
	priv->safety_enabled = (uint32_t)false;

	ret = eth_diag_unregister(priv);
	if (ret < 0) {
		netdev_err(priv->ndev, "eth_diag_unregister err\n");
	}
}

static struct eth_stl_ops hobot_eth_stl_ops = {
	.stl_init = eth_stl_init,
	.stl_stop = eth_stl_stop,
	.stl_resume = eth_stl_resume,
	.stl_open = eth_stl_open,
	.stl_extension_ioctl = eth_stl_extension_ioctl,
	.stl_update_backup_regs = eth_stl_update_backup_regs,
};

/**
 * setup_eth_stl_ops - setup stl operations for hobot_priv struct.
 * @priv: The hobot_priv struct
 * @return: NA
 */
void setup_eth_stl_ops(struct hobot_priv *priv)
{
	if (NULL != priv) {
		priv->hobot_eth_stl_ops = &hobot_eth_stl_ops;
	}
	return;
}
EXPORT_SYMBOL(setup_eth_stl_ops);/*PRQA S 0307*/
MODULE_LICENSE("GPL v2");
