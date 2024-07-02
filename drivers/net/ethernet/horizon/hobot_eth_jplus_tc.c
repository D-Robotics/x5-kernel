/*
 * Horizon Robotics
 *
 *  Copyright (C) 2020 Horizon Robotics Inc.
 *  All rights reserved.
 *
 * This is tsn function for eth driver
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <net/pkt_cls.h>
#include <net/tc_act/tc_gact.h>
#include "hobot_eth_jplus.h"

#define TIMEOUT_5MS		5
#define PER_SEC_NS		1000000000ULL
#define SZ_31				0x1fU


int tc_init(struct hobot_priv *priv)
{
	return 0;
}


/**
 * tc_setup_cbs
 * @priv: hobot private struct pointer
 * @qopt: tc_cbs_qopt_offload struct poniter
 * Description: setup cbs
 * called by eth_netdev_setup_tc
 * Return: 0: success ; otherwise error
 */
int tc_setup_cbs(struct hobot_priv *priv,
			struct tc_cbs_qopt_offload *qopt)
{
	u32 tx_queues_count = priv->plat->tx_queues_to_use;
	u32 queue = (u32)qopt->queue;
	u32 ptr, speed_div;
	u64 value;

	/* Queue 0 is not AVB capable */
	if (queue <= 0 || queue >= tx_queues_count)
		return -EINVAL;
	if (!priv->dma_cap.av)
		return -EOPNOTSUPP;

	/* Port Transmit Rate and Speed Divider */
	switch (priv->speed) {
	case SPEED_10000:
		ptr = 32;
		speed_div = 10000000;
		break;
	case SPEED_5000:
		ptr = 32;
		speed_div = 5000000;
		break;
	case SPEED_2500:
		ptr = 8;
		speed_div = 2500000;
		break;
	case SPEED_1000:
		ptr = 8;
		speed_div = 1000000;
		break;
	case SPEED_100:
		ptr = 4;
		speed_div = 100000;
		break;
	default:
		return -EOPNOTSUPP;
	}

	/* Final adjustments for HW */
	value = div_s64(qopt->idleslope * 1024ll * ptr, speed_div);/*PRQA S 4434, 1850*/
	priv->plat->tx_queues_cfg[queue].idle_slope = value & GENMASK(31, 0);/*PRQA S 4501, 0636, 0478, 1882, 4461*/

	value = div_s64(-qopt->sendslope * 1024ll * ptr, speed_div);/*PRQA S 4434, 1850*/
	priv->plat->tx_queues_cfg[queue].send_slope = value & GENMASK(31, 0);/*PRQA S 4501, 0636, 0478, 1882, 4461*/

	value = (u64)qopt->hicredit * 1024ll * 8;
	priv->plat->tx_queues_cfg[queue].high_credit = value & GENMASK(31, 0);/*PRQA S 4501, 0636, 0478, 1882, 4461*/

	value = (u64)qopt->locredit * 1024ll * 8;
	priv->plat->tx_queues_cfg[queue].low_credit = value & GENMASK(31, 0);/*PRQA S 4501, 0636, 0478, 1882, 4461*/
	/* config cbs */
	tsn_config_cbs(priv, priv->plat->tx_queues_cfg[queue].send_slope,
				priv->plat->tx_queues_cfg[queue].idle_slope,
				priv->plat->tx_queues_cfg[queue].high_credit,
				priv->plat->tx_queues_cfg[queue].low_credit,
				queue);

	dev_info(priv->device, "CBS queue %d: send %d, idle %d, hi %d, lo %d\n",
			queue, qopt->sendslope, qopt->idleslope,
			qopt->hicredit, qopt->locredit);
	return 0;
}




/**
* tsn_config_cbs
* @priv: hobot private struct pointer
* @send_slope: send slope value
* @idle_slope: idle slope value
* @high_credit: high credit value
* @low_credit: low credit value
* @queue: queue id
* Description: tsn config cbs function
* Return: NA
*/
/* code review E1: do not need to return value */
void tsn_config_cbs(const struct hobot_priv *priv, u32 send_slope,
			u32 idle_slope, u32 high_credit, u32 low_credit,
			u32 queue)
{
	void __iomem *ioaddr = priv->ioaddr;
	u32 value;

	value = ioreadl(ioaddr, MTL_ETSX_CTRL_BASE_ADDR(queue));
	value |= (u32)MTL_ETS_CTRL_AVALG;
	value |= (u32)MTL_ETS_CTRL_CC;
	iowritel(value, ioaddr, MTL_ETSX_CTRL_BASE_ADDR(queue));

	value = ioreadl(ioaddr, MTL_SEND_SLP_CREDX_BASE_ADDR(queue));
	value &= ~MTL_SEND_SLP_CRED_SSC_MASK;
	value |= send_slope & MTL_SEND_SLP_CRED_SSC_MASK;
	iowritel(value, ioaddr, MTL_SEND_SLP_CREDX_BASE_ADDR(queue));

	value = ioreadl(ioaddr, MTL_TXQX_WEIGHT_BASE_ADDR(queue));
	value &= ~MTL_TXQ_WEIGHT_ISCQW_MASK;
	value |= idle_slope & MTL_TXQ_WEIGHT_ISCQW_MASK;
	iowritel(value ,ioaddr, MTL_TXQX_WEIGHT_BASE_ADDR(queue));


	value = ioreadl(ioaddr, MTL_HIGH_CREDX_BASE_ADDR(queue));
	value &= ~MTL_HIGH_CRED_HC_MASK;
	value |= high_credit & (u32)MTL_HIGH_CRED_HC_MASK;
	iowritel(value, ioaddr, MTL_HIGH_CREDX_BASE_ADDR(queue));

	value = ioreadl(ioaddr, MTL_LOW_CREDX_BASE_ADDR(queue));
	value &= ~MTL_HIGH_CRED_LC_MASK;
	value |= low_credit & MTL_HIGH_CRED_LC_MASK;
	iowritel(value, ioaddr, MTL_LOW_CREDX_BASE_ADDR(queue));

	value = ioreadl(ioaddr, MTL_ETSX_CTRL_BASE_ADDR(queue));
	value |= (u32)MTL_ETS_CTRL_SLC;
	iowritel(value, ioaddr, MTL_ETSX_CTRL_BASE_ADDR(queue));
}



/**
 * est_write_reg
 * @priv: hobot_priv struct poniter
 * @reg: register value
 * @val: data value
 * @is_gcla: est gcla value
 * Description: write est register
 * Return: 0 on success, otherwise error.
 */
s32 est_write_reg(const struct hobot_priv *priv, u32 reg, u32 val, bool is_gcla)
{
	u32 control = 0x0;
	unsigned long timeout_jiffies;
	iowritel(val, priv->ioaddr, MTL_EST_GCL_DATA);

	control |= reg;
	control |= is_gcla ? 0x0U : (u32)MTL_EST_GCRR;
	iowritel(control, priv->ioaddr, MTL_EST_GCL_CONTROL);

	control |= (u32)MTL_EST_SRWO;
	iowritel(control, priv->ioaddr, MTL_EST_GCL_CONTROL);

	timeout_jiffies = jiffies + msecs_to_jiffies(TIMEOUT_5MS);
	while ((ioreadl(priv->ioaddr, MTL_EST_GCL_CONTROL) & MTL_EST_SRWO) != 0x0U) {
		if (time_is_before_jiffies(timeout_jiffies)) {/*PRQA S 2996*/
			/*avoid scheduling timeouts*/
			u32 regval = ioreadl(priv->ioaddr, MTL_EST_GCL_CONTROL);
			if ((regval & MTL_EST_SRWO) == 0x0U)
				break;
			netdev_err(priv->ndev, "failed to write EST reg control 0x%x\n", control);
			return -ETIMEDOUT;
		}
		mdelay(1); /*PRQA S 2880, 2877*/
	}

	return 0;
}




/* code review E1: do not need to return value */
static void tsn_config_fpe(const struct hobot_priv *priv, bool enable)
{
	u32 control;
	u32 value;


	if (!enable) {
		control = ioreadl(priv->ioaddr, GMAC_FPE_CTRL_STS);
		control &= (u32)~GMAC_FPE_EFPE;
		iowritel(control, priv->ioaddr, GMAC_FPE_CTRL_STS);
		return;
	}

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

	return;
}


/**
 * est_configuration
 * @priv: hobot_priv struct poniter
 * Description: config est function
 * Return: 0 on success, otherwise error.
 */
static s32 tsn_config_est(struct hobot_priv *priv)
{
	void __iomem *ioaddr = priv->ioaddr;
	struct est_cfg *cfg = &priv->plat->est_cfg;
	u32 control, i;
	s32 ret;
	u8 ptov = 0;


	control = ioreadl(ioaddr, MTL_EST_CONTROL);
	control &= (u32)~MTL_EST_EEST;
	iowritel(control, ioaddr, MTL_EST_CONTROL);

	if (est_write_reg(priv, MTL_EST_BTR_LOW, cfg->btr_offset[0], (bool)false) < 0) {
		netdev_err(priv->ndev, "%s:est btr low timeout\n", __func__);
		ret = -ETIMEDOUT;
		goto est_err;
	}
	if (est_write_reg(priv, (u32)MTL_EST_BTR_HIGH, cfg->btr_offset[1], (bool)false) < 0) {
		netdev_err(priv->ndev, "%s:est btr high timeout\n", __func__);
		ret = -ETIMEDOUT;
		goto est_err;
	}

	if (est_write_reg(priv, MTL_EST_CTR_LOW, cfg->ctr[0], (bool)false) < 0) {
		netdev_err(priv->ndev, "%s:est ctr low timeout\n", __func__);
		ret = -ETIMEDOUT;
		goto est_err;
	}
	if (est_write_reg(priv, MTL_EST_CTR_HIGH, cfg->ctr[1], (bool)false) < 0) {
		netdev_err(priv->ndev, "%s:est ctr high timeout\n", __func__);
		ret = -ETIMEDOUT;
		goto est_err;
	}
	if (est_write_reg(priv, MTL_EST_TER, cfg->ter, (bool)false) < 0) {
		netdev_err(priv->ndev, "%s:est ter timeout\n", __func__);
		ret = -ETIMEDOUT;
		goto est_err;
	}
	if (est_write_reg(priv, MTL_EST_LLR, cfg->gcl_size, (bool)false) < 0) {
		netdev_err(priv->ndev, "%s:est llr timeout\n", __func__);
		return -ETIMEDOUT;
		goto est_err;
	}

	for (i = 0; i < cfg->gcl_size; i++) {
		u32 reg = (i << MTL_EST_ADDR_OFFSET) & MTL_EST_ADDR;
		pr_debug("%s, %u gcl:0x%x\n", __func__, i, cfg->gcl[i]);/*PRQA S 1294, 0685*/
		if (est_write_reg(priv, reg, cfg->gcl[i], (bool)true) < 0) {
			netdev_err(priv->ndev, "%s:est gcl timeout\n", __func__);
			ret = -ETIMEDOUT;
			goto est_err;
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


	priv->est_enabled = (bool)true;
	return 0;

est_err:
	priv->est_enabled = (bool)false;
	iowritel(0, priv->ioaddr, MTL_EST_CONTROL);
	return ret;
}




s32 tc_setup_taprio(struct hobot_priv *priv, struct tc_taprio_qopt_offload *qopt)
{
	u32 size, wid, dep;
	struct timespec64 time, current_time;
	ktime_t current_time_ns;
	bool fpe = false;
	s32 i, ret = 0;
	u64 ctr;

	if ( (NULL == priv) || (NULL == qopt) ) {
		(void)pr_err("%s, dev ptr is null\n", __func__);
		return -EINVAL;
	}

	if (!qopt->enable) {
		goto disable;
	}
	dep = priv->dma_cap.estdep;
	if (qopt->num_entries >= dep) {
		netdev_err(priv->ndev, "The entry is out of range\n");
		return -EINVAL;
	}
	if (0U == qopt->cycle_time) {
		netdev_err(priv->ndev, "The cycle-time cannot be 0\n");
		return -ERANGE;
	}

	wid = priv->dma_cap.estwid;
	size = qopt->num_entries;
	priv->plat->est_cfg.gcl_size = size;
	priv->plat->est_cfg.enabled = qopt->enable;

	for (i = 0; i < size; i++) {
		s64 delta_ns = qopt->entries[i].interval;
		u32 gates = qopt->entries[i].gate_mask;

		if (delta_ns > GENMASK(wid, 0)) {
			netdev_err(priv->ndev, "The interval of entry is out of range\n");
			return -ERANGE;
		}
		if (gates > GENMASK(SZ_31 - wid, 0)) {
			netdev_err(priv->ndev, "The gatemask of entry is out of range\n");
			return -ERANGE;
		}

		switch (qopt->entries[i].command) {
		case TC_TAPRIO_CMD_SET_GATES:
			if (fpe) {
				netdev_err(priv->ndev, "only set gates command is not expected\n");
				return -EINVAL;
			}
			break;
		case TC_TAPRIO_CMD_SET_AND_HOLD:
			gates |= BIT(0);
			fpe = true;
			break;
		case TC_TAPRIO_CMD_SET_AND_RELEASE:
			gates &= ~BIT(0);
			fpe = true;
			break;
		default:
			netdev_err(priv->ndev, "invalid command\n");
			return -EOPNOTSUPP;
		}

		priv->plat->est_cfg.gcl[i] = delta_ns | (gates << wid);
	}

	/* Adjust for real system time */
	priv->ptp_clock_ops.gettime64(&priv->ptp_clock_ops, &current_time);
	current_time_ns = timespec64_to_ktime(current_time);
	if (ktime_after(qopt->base_time, current_time_ns)) {
		time = ktime_to_timespec64(qopt->base_time);
	} else {
		ktime_t base_time;
		s64 n;

		n = div64_s64(ktime_sub_ns(current_time_ns, qopt->base_time),
			      qopt->cycle_time);
		base_time = ktime_add_ns(qopt->base_time,
					 (n + 1) * qopt->cycle_time);

		time = ktime_to_timespec64(base_time);
	}

	priv->plat->est_cfg.btr_offset[0] = (u32)time.tv_nsec;
	priv->plat->est_cfg.btr_offset[1] = (u32)time.tv_sec;

	ctr = qopt->cycle_time;
	priv->plat->est_cfg.ctr[0] = do_div(ctr, NSEC_PER_SEC);
	priv->plat->est_cfg.ctr[1] = (u32)ctr;


	tsn_config_fpe(priv, (fpe || priv->plat->fp_en));

	ret = tsn_config_est(priv);
	if (ret < 0) {
		netdev_err(priv->ndev, "failed to configure EST\n");
		goto disable;
	}

	netdev_info(priv->ndev, "configured EST\n");
	return 0;

disable:
	priv->plat->est_cfg.enabled = false;
	iowritel(0, priv->ioaddr, MTL_EST_CONTROL);
	return ret;
}

