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

#include <linux/tsn.h>
#include <trace/events/tsn.h>

#include "tsn_internal.h"

#define AVTP_GPTP_TIMEMASK 0xFFFFFFFF



static u32 tsnh_avtp_timestamp(u64 ptime_ns)
{
	/* See 1722-2011, 5.4.8
	 *
	 * (AS_sec * 1e9 + AS_ns) % 2^32
	 *
	 * Just use ktime_get_ns() and grab lower 32 bits of it.
	 */
	/* u64 ns = ktime_to_ns(ktime_get()); */
	u32 gptp_ts = ptime_ns & AVTP_GPTP_TIMEMASK;
	return gptp_ts;
}


int tsnh_ch_init(struct avtp_ch *header)
{
	if (!header)
		return -EINVAL;
	header = memset(header, 0, sizeof(*header));

	/* This should be changed when setting control / data
	 * content. Set to experimental to allow for strange content
	 * should callee not do job properly
	 */
	header->subtype = TSN_EF_STREAM;

	header->version = 0;
	return 0;
}

int tsnh_validate_du_header(struct tsn_link *link, struct avtp_ch *ch,
			     struct sk_buff *skb)
{
	struct avtpdu_header *header = (struct avtpdu_header *)ch;
	struct sockaddr_ll *sll;
	u16 bytes;
	//u8 seqnr;

	if  (ch->cd && !link->is_synced && tsn_lb(link)) {
		pr_info("%s: link is now synced\n", __func__);
		link->is_synced = true;
	}

	/* As a minimum, we should match the sender's MAC to the
	 * expected MAC before we pass the frame along.
	 *
	 * This does not give much in the way of security (a malicious
	 * user could fake this), but it should remove accidents and
	 * errors.
	 */
	sll = (struct sockaddr_ll *)&skb->cb;
	sll->sll_halen = dev_parse_header(skb, sll->sll_addr);
	if (sll->sll_halen != 6)
		return -EPROTO;
	if (memcmp(link->remote_mac, &sll->sll_addr, 6))
		return -EPROTO;

	/* Current iteration of TSN has version 0b000 only */
	if (ch->version)
		return -EPROTO;

	/* Invalid StreamID, should not have ended up here in the first
	 * place (since we do DU only), if invalid sid, how did we find
	 * the link?
	 */
	if (!ch->sv)
		return -EPROTO;

	/* Check seqnr, if we have lost one frame, we _could_ insert an
	 * empty frame, but since we have frame-guarantee from 802.1Qav,
	 * we don't. Shim should handle missing frames should they occur
	 *
	 * (TODO: need to propagate seqnr to shim)
	 */
	/*seqnr = (link->last_seqnr + 1) & 0xff;
	if (header->seqnr != seqnr) {
		return -EPROTO;
	}*/

	bytes = ntohs(header->sd_len);
	if (bytes == 0 || bytes > link->max_payload_size)
		return -EINVAL;

	/* let shim validate header here as well */
	/*if (link->ops->validate_header &&
	    link->ops->validate_header(link, header) != 0)
		return -EINVAL;*/

	return 0;
}




int tsnh_assemble_du(struct tsn_link *link, struct avtpdu_header *header,
		     size_t bytes, u64 ts_pres_ns, bool cd)
{
	if (!header || !link)
		return -EINVAL;

	tsnh_ch_init((struct avtp_ch *)header);
	header->cd = cd ? 1 : 0;
	header->sv = 1;
	header->mr = 0;
	header->gv = 0;
	header->tv = 1;
	header->tu = 0;
	header->avtp_timestamp = htonl(tsnh_avtp_timestamp(ts_pres_ns));
	header->gateway_info = 0;
	header->sd_len = htons(bytes);

	if (!link->ops) {
		pr_err("%s: No available ops, cannot assemble data-unit\n",
		       __func__);
		return  -EINVAL;
	}

	header->stream_id = cpu_to_be64(link->stream_id);
	header->seqnr = link->last_seqnr++;
	link->ops->assemble_header(link, header, bytes);

	return 0;
}



int tsnh_handle_du(struct tsn_link *link, struct avtp_ch *ch)
{
	struct avtpdu_header *header = (struct avtpdu_header *)ch;
	void *data;
	u16 bytes;
	int ret;

	bytes = ntohs(header->sd_len);

	trace_tsn_du(link, bytes);
	/* bump seqnr */
	data = link->ops->get_payload_data(link, header);
	if (!data)
		return -EINVAL;

	link->last_seqnr = header->seqnr;
	ret = tsn_buffer_write_net(link, data, bytes);
	if (ret != bytes)
		return ret;

	return 0;
}
