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

#ifndef _TSN_INTERNAL_H_
#define _TSN_INTERNAL_H_
#include <linux/tsn.h>

#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>



struct avtpdu_header;
struct tsn_link;
struct tsn_shim_ops;



#define IS_TSN_FRAME(x) (ntohs(x) == ETH_P_TSN)
#define IS_PTP_FRAME(x) (ntohs(x) == ETH_P_1588)
#define IS_1Q_FRAME(x)  (ntohs(x) == ETH_P_8021Q)



struct tsn_link *tsn_create_and_add_link(struct tsn_nic *nic);



ssize_t tsn_get_stream_ids(char *page, ssize_t len);



struct tsn_link *tsn_find_by_stream_id(u64 sid);

void tsn_readd_link(struct tsn_link *link, u64 old_key);

void tsn_remove_link(struct tsn_link *link);

void tsn_remove_and_free_link(struct tsn_link *link);

int tsn_set_shim_ops(struct tsn_link *link, struct tsn_shim_ops *shim_ops);

int tsn_prepare_link(struct tsn_link *link);
int tsn_teardown_link(struct tsn_link *link);


void *tsn_set_external_buffer(struct tsn_link *link, void *buffer,
			      size_t buffer_size);


int tsn_buffer_write_net(struct tsn_link *link, void *src, size_t bytes);


int tsn_buffer_read_net(struct tsn_link *link, void *buffer, size_t bytes);


static inline int tsn_core_running(struct tsn_list *list)
{
	if (list)
		return atomic_read(&list->running);
	return 0;
}


static inline size_t _tsn_buffer_used(struct tsn_link *link)
{
	return  (link->head - link->tail) % link->used_buffer_size;
}



int tsn_configfs_init(struct tsn_list *tlist);
void tsn_configfs_exit(struct tsn_list *tlist);


static inline size_t tsnh_len(void)
{
	/* include 802.1Q tag */
	return sizeof(struct avtpdu_header);
}

static inline u16 tsnh_len_all(void)
{
	return (u16)tsnh_len() + VLAN_ETH_HLEN;
}



static inline u16 tsnh_frame_len(struct tsn_link *link)
{
	if (!link)
		return 0;
	pr_info("max_payload_size=%u, shim_header_size=%u, tsnh_len_all()=%u\n",
		link->max_payload_size, link->shim_header_size, tsnh_len_all());
	return link->max_payload_size + link->shim_header_size + tsnh_len_all();
}


static inline u16 tsnh_data_len(struct avtpdu_header *header)
{
	if (!header)
		return 0;
	return ntohs(header->sd_len);
}



static inline int tsnh_payload_size_valid(u16 max_payload_size,
					  u16 shim_hdr_size)
{
	/* VLAN_ETH_ZLEN	64 */
	/* VLAN_ETH_FRAME_LEN	1518 */
	u32 framesize = max_payload_size + tsnh_len_all() + shim_hdr_size;

	return framesize >= VLAN_ETH_ZLEN && framesize <= VLAN_ETH_FRAME_LEN;
}



int tsnh_validate_du_header(struct tsn_link *link, struct avtp_ch *ch,
			    struct sk_buff *skb);



int tsnh_assemble_du(struct tsn_link *link, struct avtpdu_header *header,
		     size_t bytes, u64 ts_pres_ns, bool cd);



int tsnh_handle_du(struct tsn_link *link, struct avtp_ch *ch);



static inline struct avtp_ch *tsnh_ch_from_skb(struct sk_buff *skb)
{
	if (!skb)
		return NULL;
	if (!IS_TSN_FRAME(eth_hdr(skb)->h_proto))
		return NULL;

	return (struct avtp_ch *)skb->data;
}


int tsn_net_add_rx(struct tsn_list *list);


void tsn_net_remove_rx(struct tsn_list *list);


int tsn_net_prepare_tx(struct tsn_list *tlist);


void tsn_net_disable_tx(struct tsn_list *tlist);

void tsn_net_close(struct tsn_link *link);


int tsn_net_send_set(struct tsn_link *link, size_t num, u64 ts_base_ns,
		     u64 ts_delta_ns, size_t *sent_bytes);

#endif	/* _TSN_INTERNAL_H_ */
