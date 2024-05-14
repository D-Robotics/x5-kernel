/*
 *   TSN Core main part of TSN driver
 *
 *   Copyright (C) 2015- Henrik Austad <haustad@cisco.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/socket.h>
#include <linux/skbuff.h>
#include <linux/if_vlan.h>
#include <linux/skbuff.h>
#include <net/sock.h>

#include <linux/tsn.h>
#include <trace/events/tsn.h>
#include "tsn_internal.h"




static rx_handler_result_t tsn_rx_handler(struct sk_buff **pskb)
{
	struct sk_buff *skb = *pskb;
	const struct ethhdr *ethhdr = eth_hdr(skb);
	struct avtp_ch *ch;
	struct tsn_link *link;
	rx_handler_result_t ret = RX_HANDLER_PASS;

	ch = tsnh_ch_from_skb(skb);
	if (!ch)
		return RX_HANDLER_PASS;

	link = tsn_find_by_stream_id(be64_to_cpu(ch->stream_id));
	if (!link)
		return RX_HANDLER_PASS;

	tsn_lock(link);

	if (!tsn_link_is_on(link) || link->estype_talker)
		goto out_unlock;

	/* If link->ops is not set yet, there's nothing we can do, just
	 * ignore this frame
	 */
	if (!link->ops)
		goto out_unlock;

	if (tsnh_validate_du_header(link, ch, skb))
		goto out_unlock;

	/* Update link network time
	 *
	 * TODO: using the time in the skb is flawed, we should use
	 * actual time from the NIC and then correlate that to timestamp
	 * in frame.
	 */
	tsn_update_net_time(link, ktime_to_ns(skb_get_ktime(skb)), 1);
	trace_tsn_rx_handler(link, ethhdr, be64_to_cpu(ch->stream_id));

	/* Handle dataunit, if it failes, pass on the frame and let
	 * userspace pick it up.
	 */
	if (tsnh_handle_du(link, ch) < 0)
		goto out_unlock;

	/* Done, data has been copied, free skb and return consumed */
	consume_skb(skb);
	ret = RX_HANDLER_CONSUMED;

out_unlock:
	tsn_unlock(link);
	return ret;
}




int tsn_net_add_rx(struct tsn_list *tlist)
{
	struct tsn_nic *nic;

	if (!tlist)
		return -EINVAL;

	/* Setup receive handler for TSN traffic.
	 *
	 * Receive will happen all the time, once a link is active as a
	 * Listener, we will add a hook into the receive-handler to
	 * steer the frames to the correct link.
	 *
	 * We try to add Rx-handlers to all the card listed in tlist (we
	 * assume core has filtered the NICs appropriatetly sothat only
	 * TSN-capable cards are present).
	 */
	rtnl_lock();
	tsn_list_lock(tlist);
	list_for_each_entry(nic, &tlist->head, list) {
		if (netdev_rx_handler_register(nic->dev, tsn_rx_handler, nic) < 0) {
			pr_err("%s: could not attach an Rx-handler to %s, this link will not be able to accept TSN traffic\n",
			       __func__, nic->name);
			continue;
		}
		pr_info("%s: attached rx-handler to %s\n",
			__func__, nic->name);
		nic->rx_registered = 1;
	}
	tsn_list_unlock(tlist);
	rtnl_unlock();
	return 0;
}




void tsn_net_remove_rx(struct tsn_list *tlist)
{
	struct tsn_nic *nic;

	if (!tlist)
		return;
	rtnl_lock();
	tsn_list_lock(tlist);
	list_for_each_entry(nic, &tlist->head, list) {
		if (nic->rx_registered)
			netdev_rx_handler_unregister(nic->dev);
		nic->rx_registered = 0;
		pr_info("%s: RX-handler for %s removed\n",
			__func__, nic->name);
	}
	tsn_list_unlock(tlist);
	rtnl_unlock();
}



int tsn_net_prepare_tx(struct tsn_list *tlist)
{
	struct tsn_nic *nic;
	struct device *dev;
	int ret = 0;

	if (!tlist)
		return -EINVAL;

	tsn_list_lock(tlist);
	list_for_each_entry(nic, &tlist->head, list) {
		if (!nic)
			continue;
		if (!nic->capable)
			continue;

		if (!nic->dev->netdev_ops)
			continue;

		dev = nic->dev->dev.parent;
		/*nic->dma_mem = dma_alloc_coherent(dev, nic->dma_size,
						  &nic->dma_handle, GFP_ATOMIC);
		if (!nic->dma_mem) {
			nic->capable = 0;
			nic->dma_size = 0;
			continue;
		}*/
		ret++;
	}
	tsn_list_unlock(tlist);
	pr_info("%s: configured %d cards to use DMA\n", __func__, ret);
	return ret;
}


void tsn_net_disable_tx(struct tsn_list *tlist)
{
	struct tsn_nic *nic;
	struct device *dev;
	int res = 0;

	if (!tlist)
		return;
	tsn_list_lock(tlist);
	list_for_each_entry(nic, &tlist->head, list) {
		if (nic->capable && nic->dma_mem) {
			dev = nic->dev->dev.parent;
			dma_free_coherent(dev, nic->dma_size, nic->dma_mem,
					  nic->dma_handle);
			res++;
		}
	}
	tsn_list_unlock(tlist);
	pr_info("%s: freed DMA regions from %d cards\n", __func__, res);
}



void tsn_net_close(struct tsn_link *link)
{
	/* struct tsn_rx_handler_data *rx_data; */

	/* Careful! we need to make sure that we actually succeeded in
	 * registering the handler in open unless we want to unregister
	 * some random rx_handler..
	 */
	if (!link->estype_talker) {
		;
		/* Make sure we notify rx-handler so it doesn't write
		 * into NULL
		 */
	}
}


static inline u16 _get_8021q_vid(struct tsn_link *link)
{
	u16 pcp = sr_class_to_pcp(link->nic, link->class);
	/* If not explicitly provided, use SR_PVID 0x2*/
	return (link->vlan_id & VLAN_VID_MASK) | ((pcp & 0x7) << 13);
}


#if 0
static u16 __tsn_pick_tx(struct net_device *dev, struct sk_buff *skb)
{
	printk_once(KERN_ERR "TSN ERROR: sending frame via NIC without valid ndo_select_queue, defaulting to tx-ring 0\n");
	return 0;
}
#endif



static struct sk_buff *_skbuf_create_init(struct tsn_link *link,
					  size_t data_bytes,
					  size_t shim_hdr_size,
					  u64 ts_pres_ns,
					  bool cd)
{
	struct sk_buff *skb = NULL;
	struct avtpdu_header *avtpdu;
	struct net_device *dev = link->nic->dev;
	int res = 0;
	u16 queue_index = 0;
	size_t hdr_len = VLAN_ETH_HLEN;
	size_t avtpdu_len = tsnh_len() + shim_hdr_size;
	u16 vlan_tci = _get_8021q_vid(link);

	if (data_bytes > link->used_buffer_size) {
		printk_once(KERN_ERR "%s: data_bytes (%zu) exceed buffer-size (%zd), reducing size\n",
			__func__,data_bytes, link->used_buffer_size);
		data_bytes = link->used_buffer_size;
	}

	skb = alloc_skb(hdr_len + avtpdu_len + data_bytes + dev->needed_tailroom,
			GFP_ATOMIC);
	if (!skb)
		return NULL;

	skb_reserve(skb, hdr_len + avtpdu_len);
	skb->dev = link->nic->dev;
	skb_reset_mac_header(skb);
	skb->network_header = skb->mac_header + VLAN_ETH_HLEN;
	skb->priority = sr_class_to_pcp(link->nic, link->class);

	/* copy shim-data
	 *
	 * This all hinges on that the shim-header size is set correctly
	 * via configfs, if that value is off, then this will fall
	 * apart.
	 */
	res = tsn_buffer_read_net(link, skb_put(skb, data_bytes), data_bytes);
	if (res != data_bytes) {
		pr_err("%s: Could not copy %zd bytes of data. Res: %d\n",
		       __func__, data_bytes, res);
		kfree_skb(skb);
		return NULL;
	}

	/* set avtpdu- && shim-header.
	 * data_bytes is requried to set fields of header correctly
	 */
	avtpdu = (struct avtpdu_header *)skb_push(skb, avtpdu_len);
	res = tsnh_assemble_du(link, avtpdu, data_bytes, ts_pres_ns, cd);
	if (res < 0) {
		pr_err("%s: Error initializing header (-> %d) , we are in an inconsistent state!\n",
		       __func__, res);
		kfree_skb(skb);
		return NULL;
	}

	/* set ethenet header */
	res = skb_vlan_push(skb, htons(ETH_P_8021Q), vlan_tci);
	if (res) {
		pr_err("%s: could not insert tag (0x%04x) && proto (0x%04x) in buffer, aborting -> %d\n",
		       __func__, vlan_tci, htons(ETH_P_8021Q), res);
		return NULL;
	}

	skb->protocol = htons(ETH_P_TSN);
	skb->pkt_type = PACKET_OUTGOING;

	skb_shinfo(skb)->tx_flags |= SKBTX_HW_TSTAMP;
	skb_set_mac_header(skb, 0);

	/* We are using a ethernet-type frame (even though we could send
	 * TSN over other medium.
	 *
	 * - skb_push(skb, ETH_HLEN)
	 * - set header htons(header)
	 * - set source addr (netdev mac addr)
	 * - set dest addr
	 * - return ETH_HLEN
	 */
	if (!dev_hard_header(skb, skb->dev, ETH_P_TSN, link->remote_mac, NULL, 6)) {
		pr_err("%s: could not copy remote MAC to ether-frame\n", __func__);
		kfree(skb);
		return NULL;
	}

	/* Set txqueue, must set queue_mapping, via ndo_select_queue */
	if (dev->netdev_ops->ndo_select_queue)
		queue_index = dev->netdev_ops->ndo_select_queue(dev, skb, NULL);
	queue_index = netdev_cap_txqueue(dev, queue_index);
	skb_set_queue_mapping(skb, queue_index);

	skb->csum = skb_checksum(skb, 0, hdr_len + data_bytes, 0);
	return skb;
}




int tsn_net_send_packet(struct tsn_link *link, size_t payload_size,
		u64 ts, bool cd)
{
	struct sk_buff *skb = _skbuf_create_init(link, payload_size,
			tsn_shim_get_hdr_size(link), ts, cd);
	int res;

	if (!skb) {
		pr_err("%s: could no allocate memory for skb\n", __func__);
		return -ENOMEM;
	}

	trace_tsn_pre_tx(link, skb, payload_size);
	res = dev_queue_xmit(skb);
	if (res != NET_XMIT_SUCCESS) {
		printk_once(KERN_WARNING "TSN ERROR: dev_queue_xmit() FAILED -> %d\n", res);
		return -EIO;
	}

	return 0;
}


int tsn_net_send_frame(struct tsn_link *link, size_t size, u64 ts)
{
	unsigned int i, packet_count = rounddown(size, link->max_payload_size) /
		link->max_payload_size;
	size_t packet_size_rem = size % link->max_payload_size;
	int sent_packets = 0;
	bool first = true;

	for (i = 0; i < packet_count; i++) {
		tsn_net_send_packet(link, link->max_payload_size, ts, first);
		ts++;
		sent_packets++;
		first = false;
	}

	if (packet_size_rem) {
		tsn_net_send_packet(link, packet_size_rem, ts, first);
		ts++;
		sent_packets++;
	}

	return sent_packets;
}


int tsn_net_send_set(struct tsn_link *link, size_t num, u64 ts_base_ns,
		u64 ts_delta_ns, size_t *sent_bytes)
{
	u64 ts_pres_ns = ts_base_ns;
	struct net_device *dev;
	size_t data_size;
	size_t sent = 0;
	int ret;

	if (!link)
		return -EINVAL;

	dev = link->nic->dev;
	data_size = tsn_shim_get_framesize(link);

	while (sent < num) {
		ret = tsn_net_send_frame(link, data_size, ts_pres_ns);
		if (ret <= 0)
			goto out;

		if (link->ops->copy_done)
			link->ops->copy_done(link);

		ts_pres_ns += ts_delta_ns;
		sent += ret;

		if (sent_bytes)
			*sent_bytes += data_size;
	}

out:
	trace_tsn_post_tx_set(link, sent);
	return sent;
}
