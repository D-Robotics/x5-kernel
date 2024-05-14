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

#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched.h>
#include <linux/hashtable.h>
#include <linux/netdevice.h>
#include <linux/net.h>
#include <linux/dma-mapping.h>
#include <net/sock.h>
#include <net/net_namespace.h>
#include <linux/hrtimer.h>
#include <linux/configfs.h>
#include <linux/ktime.h>

#define CREATE_TRACE_POINTS
#include <trace/events/tsn.h>
#include "tsn_internal.h"

static struct tsn_list tlist;
static int in_debug;
static int on_cpu = -1;
static bool tsn_enabled = false;







#define TLINK_HASH_BITS 8
DEFINE_HASHTABLE(tlinks, TLINK_HASH_BITS);

static LIST_HEAD(tsn_shim_ops);


#define LOW_WATER_MS 20
static inline size_t _get_low_water(struct tsn_link *link)
{
	int numframes = LOW_WATER_MS * (link->class == SR_CLASS_A ? 8 : 4);

	return link->max_payload_size * numframes;
}




static inline size_t _get_high_water(struct tsn_link *link)
{
	size_t low_water =  _get_low_water(link);

	return max(link->used_buffer_size, low_water);
}



static int _tsn_set_buffer(struct tsn_link *link, void *buffer, size_t bufsize)
{
	if (link->buffer) {
		pr_err("%s: Cannot add buffer, buffer already registred\n",
		       __func__);
		return -EINVAL;
	}

	trace_tsn_set_buffer(link, bufsize);
	link->buffer = buffer;
	link->head = link->buffer;
	link->tail = link->buffer;
	link->end = link->buffer + bufsize;
	link->buffer_size = bufsize;
	link->used_buffer_size = bufsize;
	return 0;
}



static void _tsn_free_buffer(struct tsn_link *link)
{
	if (!link)
		return;
	trace_tsn_free_buffer(link, link->buffer_size);
	kfree(link->buffer);
	link->buffer = NULL;
	link->head   = NULL;
	link->tail   = NULL;
	link->end    = NULL;
}




int tsn_set_buffer_size(struct tsn_link *link, size_t bsize)
{
	if (!link)
		return -EINVAL;

	if (bsize > link->buffer_size) {
		pr_err("%s: requested buffer (%zd) larger than allocated memory (%zd)\n",
		       __func__, bsize, link->buffer_size);
		return -ENOMEM;
	}

	tsn_lock(link);
	link->used_buffer_size = bsize;
	link->tail = link->buffer;
	link->head = link->buffer;
	link->end = link->buffer + link->buffer_size;
	link->low_water_mark = _get_low_water(link);
	link->high_water_mark = _get_high_water(link);
	tsn_unlock(link);

	pr_info("Set buffer_size, size: %zd, lowwater: %zd, highwater: %zd\n",
		link->used_buffer_size, link->low_water_mark,
		link->high_water_mark);
	return 0;
}
EXPORT_SYMBOL(tsn_set_buffer_size);


int tsn_clear_buffer_size(struct tsn_link *link)
{
	if (!link)
		return -EINVAL;

	tsn_lock(link);
	link->tail = link->buffer;
	link->head = link->buffer;
	link->end = link->buffer + link->buffer_size;
	memset(link->buffer, 0,  link->used_buffer_size);
	link->used_buffer_size = link->buffer_size;
	link->low_water_mark = _get_low_water(link);
	link->high_water_mark = _get_high_water(link);
	tsn_unlock(link);
	return 0;
}
EXPORT_SYMBOL(tsn_clear_buffer_size);

void *tsn_set_external_buffer(struct tsn_link *link, void *buffer,
			      size_t buffer_size)
{
	void *old_buffer;

	if (!link)
		return NULL;
	if (buffer_size < link->max_payload_size)
		pr_warn("%s: buffer_size (%zu) < max_payload_size (%u)\n",
			__func__, buffer_size, link->max_payload_size);

	tsn_lock(link);
	if (!link->external_buffer && link->buffer)
		_tsn_free_buffer(link);

	old_buffer = link->buffer;
	link->external_buffer = 1;
	link->buffer_size = buffer_size;
	link->used_buffer_size = buffer_size;
	link->buffer = buffer;
	link->head = link->buffer;
	link->tail = link->buffer;
	link->end = link->buffer + link->used_buffer_size;
	tsn_unlock(link);
	return old_buffer;
}
EXPORT_SYMBOL(tsn_set_external_buffer);



static void __tsn_buffer_write(struct tsn_link *link, void *src, size_t bytes)
{
	int rem = 0;

	/* No Need To Wrap, if overflow we will overwrite without
	 * warning.
	 */
	trace_tsn_buffer_write(link, bytes);
	if (link->head + bytes < link->end) {
		memcpy(link->head, src, bytes);
		link->head += bytes;
	} else {
		rem = link->end - link->head;
		memcpy(link->head, src, rem);
		memcpy(link->buffer, (src + rem), bytes - rem);
		link->head = link->buffer + (bytes - rem);
	}
}


int tsn_buffer_write(struct tsn_link *link, void *src, size_t bytes)
{
	if (!link)
		return -EINVAL;

	if (tsn_link_is_err(link)) {
		tsn_teardown_link(link);
		return -EIO;
	}

	/* We should not do anything if link has gone inactive */
	if (!tsn_link_is_on(link))
		return 0;

	tsn_lock(link);
	if ((link->available_bytes + bytes) <= link->buffer_size) {
		__tsn_buffer_write(link, src, bytes);
		link->available_bytes += bytes;
	}
	tsn_unlock(link);

	/* Copied a batch of data and if link is disabled, it is now
	 * safe to enable it. Otherwise we will continue to send
	 * null-frames to remote.
	 */
	if (!tsn_lb(link))
		tsn_lb_enable(link);

	return bytes;
}
EXPORT_SYMBOL(tsn_buffer_write);

int tsn_buffer_write_net(struct tsn_link *link, void *src, size_t bytes)
{
	size_t used;

	if (!link)
		return -EINVAL;

	/* Driver has not been enabled yet, i.e. it is in state 'off' and we
	 * have no way of knowing the state of the buffers.
	 * Silently drop the data, pretend write went ok
	 */
	trace_tsn_buffer_write_net(link, bytes);
	if (!tsn_lb(link))
		return bytes;
	if (!link->is_synced)
		return bytes;

	if ((link->available_bytes + bytes) <= link->buffer_size) {
		__tsn_buffer_write(link, src, bytes);
		link->available_bytes += bytes;
	}

	/* If we stored more data than high_water, we need to drain
	 *
	 * In ALSA, this will trigger a snd_pcm_period_elapsed() for the
	 * substream connected to this particular link.
	 */
	used = link->available_bytes;
	if (used >= link->high_water_mark) {
		trace_tsn_buffer_drain(link, used);
		link->ops->buffer_drain(link);
	}

	return bytes;
}

int tsn_update_net_time(struct tsn_link *link, u64 time_ns, int increment)
{
	u64 delta_ns;
	u64 exp_avg;

	if (time_ns < link->ts_net_ns || increment < 1)
		return -EINVAL;

	/* if increment > 1, we have sent a batch of frames, and */
	delta_ns = time_ns - link->ts_net_ns;
	if (increment > 1)
		delta_ns = div_u64(delta_ns, increment);
	do {
		exp_avg = ((delta_ns * link->ts_exp_alpha) >> 14) + ((((1 << 14) - link->ts_exp_alpha) * link->ts_exp_avg) >> 14);
		link->ts_exp_avg = exp_avg;
	} while (--increment > 0);

	link->ts_net_ns = time_ns;
	link->ts_delta_ns = delta_ns;
	trace_tsn_update_net_time(link);
	return 0;
}



static void __tsn_buffer_read(struct tsn_link *link, void *dst, size_t bytes)
{
	int rem = 0;

	trace_tsn_buffer_read(link, bytes);
	if ((link->tail + bytes) < link->end) {
		memcpy(dst, link->tail, bytes);
		link->tail += bytes;
	} else {
		rem = link->end - link->tail;
		memcpy(dst, link->tail, rem);
		memcpy(dst + rem, link->buffer, bytes - rem);
		link->tail = link->buffer + bytes - rem;
	}
}

int tsn_buffer_read_net(struct tsn_link *link, void *buffer, size_t bytes)
{
	size_t used;

	if (!link)
		return -EINVAL;

	/* link is currently inactive, e.g. we send frames, but without
	 * content. This is a debug-feature, if we don't have data to
	 * send, we should not send zero-frames.
	 *
	 * This can be done before we ship data, or if we are muted
	 * (without expressively stating that over 1722.1
	 */
	if (!tsn_lb(link)) {
		memset(buffer, 0, bytes);
		goto out;
	}

	__tsn_buffer_read(link, buffer, bytes);

	/* Trigger refill from client app */
	used = _tsn_buffer_used(link);
	if (used < link->low_water_mark) {
		trace_tsn_refill(link, used);
		link->ops->buffer_refill(link);
	}
out:
	return bytes;
}


int tsn_buffer_read(struct tsn_link *link, void *buffer, size_t bytes)
{
	if (!link)
		return -EINVAL;

	/* We should not do anything if link has gone inactive */
	if (!tsn_link_is_on(link))
		return 0;
	if (!tsn_lb(link)) {
		memset(buffer, 0, bytes);
		return bytes;
	}

	if (link->available_bytes >= bytes) {
		__tsn_buffer_read(link, buffer, bytes);
		link->available_bytes -= bytes;
	}

	return bytes;
}
EXPORT_SYMBOL(tsn_buffer_read);


static int _tsn_send_batch(struct tsn_link *link)
{
	int ret = 0;
	size_t num_frames = (link->class == SR_CLASS_A ? 8 : 4);
	u64 ts_base_ns = ktime_to_ns(ktime_get()) + (link->class == SR_CLASS_A ? 2000000 : 50000000);
	u64 ts_delta_ns = (link->class == SR_CLASS_A ? 125000 : 250000);
	size_t sent_bytes = 0, expected_sent_bytes;

	/* SNPS: should avoid avalanche without data */
	if (!tsn_lb(link))
		return 0;
	/*
	 * SNPS: No need to send multiple frames now that tsn_net.c has logic
	 * to split frames.
	 */
	num_frames = 1;
	expected_sent_bytes = tsn_shim_get_framesize(link) * num_frames;
	if (link->available_bytes < expected_sent_bytes)
		return 0;
	/* end */

	trace_tsn_send_batch(link, num_frames, ts_base_ns, ts_delta_ns);
	ret = tsn_net_send_set(link, num_frames, ts_base_ns, ts_delta_ns,
			&sent_bytes);
	if (ret < 0)
		return ret;
	link->frames_sent += ret;
	link->available_bytes -= sent_bytes;

	/* we sent ret number of frames, update timestamp with that. */
	tsn_update_net_time(link, ktime_to_ns(ktime_get()), ret);

	return 0;
}



static int tsn_worker_fn(void *data)
{
	struct tsn_link *link;
	struct hlist_node *tmp;
	int bkt = 0;
	int err;
	struct sched_param param = { .sched_priority = 10 };
	struct tsn_list *list = (struct tsn_list *)data;
	if (!data)
		return -EINVAL;

	/* FIXME: set affinity */
	/* set sched_rr and prio */
	sched_setscheduler(current, SCHED_RR, &param);

	pr_info("tsn_worker ready to run\n");
	while (!kthread_should_stop() && tsn_core_running(list)) {
		if (list->should_run <= 0) {
		sched_out:
			/* task_interruptible */
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
		}
		if (!tsn_core_running(list) || kthread_should_stop())
			break;

		if (list->should_run <= 0)
			goto sched_out;


		hash_for_each_safe(tlinks, bkt, tmp, link, node) {
			/* use the periodic wakeup to test if any of the
			 * links has failed. If it has, clear it and
			 * continue */
			if (tsn_link_is_err(link)) {
				tsn_teardown_link(link);
				continue;
			}

			/* if the link is not on, we can ignore the link
			 * for one iteration before we start sending
			 * frames, we accept this race. */
			if (!tsn_link_is_on(link))
				continue;

			tsn_lock(link);

			/* In case we are killed while waiting for the lock */
			if (kthread_should_stop()) {
				tsn_unlock(link);
				return 0;
			}

			/* FIXME: this should iterate over each link and
			 * send *one* frame pr link until all links are
			 * exhausted for this period, otherwise the
			 * first link in a run will starve the other
			 * links.
			 */
			if (tsn_link_is_on(link) && link->estype_talker) {
				err = _tsn_send_batch(link);
				if (err)
					tsn_link_err(link);
			}

			tsn_unlock(link);
		}
		list->should_run--;
	}
	pr_info("tsn_worker_fn done, wrapping up and dying\n");
	return 0;
}



static int tsn_worker_init(struct tsn_list *list)
{
	/* create wait-queue */
	list->tsn_thread = kthread_create(tsn_worker_fn, list, "tsn_worker");
	if (IS_ERR(list->tsn_thread))
		return -ENOMEM;

	/* prod the thread to make it ready if hrtimer calls it immediately */
	wake_up_process(list->tsn_thread);
	return 0;
}


static void tsn_worker_exit(struct tsn_list *list)
{
	atomic_set(&list->running, 0);
	list->should_run = 0;
	kthread_stop(list->tsn_thread);
}


static enum hrtimer_restart tsn_hrtimer_callback(struct hrtimer *hrt)
{
	struct tsn_list *list = container_of(hrt, struct tsn_list, tsn_timer);
	if (!tsn_core_running(list))
		return HRTIMER_NORESTART;

	hrtimer_forward_now(hrt, ns_to_ktime(list->period_ns));

	/* tsn_thread ready? */
	if (tsn_core_running(list)) {
		/* kick worker */
		list->should_run++;
		if (list->tsn_thread->__state != TASK_RUNNING) {
			wake_up_process(list->tsn_thread);
		}
    }

	return HRTIMER_RESTART;
}




static long tsn_hrtimer_init(struct tsn_list *list)
{
	/* Run every 1ms, _tsn_send_batch will figure out how many
	 * frames to send for active frames
	 */
	hrtimer_init(&list->tsn_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL | HRTIMER_MODE_PINNED);

	list->tsn_timer.function = tsn_hrtimer_callback;
	hrtimer_cancel(&list->tsn_timer);

	hrtimer_start(&list->tsn_timer, ns_to_ktime(list->period_ns),
		      HRTIMER_MODE_REL);

	atomic_set(&list->running, 1);
	return 0;
}

static void tsn_hrtimer_exit(struct tsn_list *list)
{
	atomic_set(&list->running, 0);
	hrtimer_cancel(&list->tsn_timer);
}

int tsn_set_shim_ops(struct tsn_link *link, struct tsn_shim_ops *shim_ops)
{
	if (!link)
		return -EINVAL;
	if (tsn_link_is_on(link))
		return -EINVAL;

	tsn_lock(link);
	link->ops = shim_ops;
	tsn_unlock(link);
	return 0;
}



int tsn_prepare_link(struct tsn_link *link)
{
	int ret = 0;
	void *buffer;
	struct net_device *netdev;

	/* TODO: use separate buckets (lists/rbtrees/whatever) for
	 * SR_CLASS_A and SR_CLASS_B talker streams. hrtimer-callback should
	 * not iterate over all.
	 */

	if (!link || !link->ops) {
		pr_err("TSN ERROR: link (%p) or link->ops (%p) not set\n",
		       link, link ? link->ops : NULL);
		return -EINVAL;
	}


	/* configure will calculate idle_slope based on framesize
	 * (header + payload)
	 *
	 * Only do this if NIC is capable (ie, in_debug and a standard NIC)
	 */
	netdev = link->nic->dev;
	if (link->nic->capable) {
		pr_info("NIC has tsn_link_configre()\n");
		tsn_lock(link);
		ret = netdev->netdev_ops->ndo_tsn_link_configure(netdev, link->class,
								 tsnh_frame_len(link), link->vlan_id & 0xfff, 1, link->nic->pcp_a, link->nic->pcp_b);
		tsn_unlock(link);
		/* NICs that have TSN support but has not enabled it
		 * will fail at this stage when we load tsn with
		 * in_debug=1.
		 */
		if (ret < 0)
			goto err_out;
	}

	ret = link->ops->probe(link);
	if (ret != 0) {
		pr_err("%s: Could not probe shim (%d), cannot create link\n",
		       __func__, ret);
		goto err_out;
	}

	if (!link->external_buffer) {
		pr_info("TSN: allocating buffer, %zd bytes\n", link->buffer_size);
		buffer = kmalloc(link->buffer_size, GFP_KERNEL);
		if (!buffer) {
			pr_err("%s: Could not allocate memory (%zu) for buffer\n",
			       __func__, link->buffer_size);
			ret = -ENOMEM;
			goto err_out;
		}

		tsn_lock(link);
		ret = _tsn_set_buffer(link, buffer, link->buffer_size);
		tsn_unlock(link);
		if (ret != 0) {
			pr_err("%s: Could not set buffer for TSN, got %d\n",
			       __func__, ret);
			goto err_out;
		}
	} else {
		/* FIXME: not handled */
		pr_info("TSN does not currently handle externally hosted buffers. This is on the TODO-list\n");
		ret = -EINVAL;
		goto err_out;
	}

	pr_info("Link is ready, marking it ON\n");
	tsn_link_on(link);
	return 0;

err_out:
	tsn_lock(link);
	link->ops = NULL;
	tsn_unlock(link);
	pr_info("%s: FAILED - ret=%d\n", __func__, ret);
	return ret;
}



int tsn_teardown_link(struct tsn_link *link)
{
	struct net_device *netdev;
	u64 frames_sent;
	int ret = 0;

	if (!link)
		return -EINVAL;

	/* Careful dance, we currently grab this lock from hrtimer, so
	 * make sure we grab the lock and disable the link quickly
	 */
	tsn_lock(link);
	tsn_link_off(link);
	tsn_lb_disable(link);
	tsn_unlock(link);

	/* Need to call media_close() without (spin-)locks held */
	if (link->ops && link->ops->media_close)
		link->ops->media_close(link);

	/* we can now grab the link and not worry about blocking hrtimer
	 * callback as link has been disabled, i.e. we will stop sending frames to the network layer
	 */
	pr_info("%s: closed shim, dropping rest of link\n", __func__);
	tsn_lock(link);

	if (!link->nic) {
		pr_err("ERROR %s: link link->nic (%p) got yanked away, cannot tear down link properly\n",
			__func__, link->nic);
		tsn_unlock(link);
		return 0;
	}

	netdev = link->nic->dev;
	if (netdev->netdev_ops->ndo_tsn_link_configure) {
		/* NOTE: this needs to be serialized
		 */
		ret = netdev->netdev_ops->ndo_tsn_link_configure(netdev, link->class, tsnh_frame_len(link), link->vlan_id & 0XFFF, 0, link->nic->pcp_a, link->nic->pcp_b);
		if (ret < 0)
			pr_err("Could not de-configure link - %d\n", ret);
	}
	frames_sent = link->frames_sent;
	link->frames_sent = 0;
	link->ops = NULL;
	_tsn_free_buffer(link);
	tsn_unlock(link);

	pr_info("%s: disabling all parts of link, %llu frames sent in total\n", __func__, frames_sent);
	return 0;
}

int tsn_shim_register_ops(struct tsn_shim_ops *shim_ops)
{
	if (!shim_ops)
		return -EINVAL;

	if (!shim_ops->buffer_refill || !shim_ops->buffer_drain ||
	    !shim_ops->media_close || !shim_ops->copy_size ||
	    !shim_ops->validate_header || !shim_ops->assemble_header ||
	    !shim_ops->get_payload_data)
		return -EINVAL;

	INIT_LIST_HEAD(&shim_ops->head);
	list_add_tail(&shim_ops->head, &tsn_shim_ops);

	return 0;
}
EXPORT_SYMBOL(tsn_shim_register_ops);


void tsn_shim_deregister_ops(struct tsn_shim_ops *shim_ops)
{
	struct tsn_link *link;
	struct hlist_node *tmp;
	int bkt;

	hash_for_each_safe(tlinks, bkt, tmp, link, node) {
		if (!link)
			continue;
		if (link->ops == shim_ops)
			tsn_teardown_link(link);
	}
	list_del(&shim_ops->head);
}
EXPORT_SYMBOL(tsn_shim_deregister_ops);



char *tsn_shim_get_active(struct tsn_link *link)
{
	if (!link || !link->ops)
		return "None";
	return link->ops->shim_name;
}


struct tsn_shim_ops *tsn_shim_find_by_name(const char *name)
{
	struct tsn_shim_ops *ops;

	if (!name || list_empty(&tsn_shim_ops))
		return NULL;

	list_for_each_entry(ops, &tsn_shim_ops, head) {
		if (strcmp(name, ops->shim_name) == 0)
			return ops;
	}
	return NULL;
}


ssize_t tsn_shim_export_probe_triggers(char *page)
{
	struct tsn_shim_ops *ops;
	ssize_t res = 0;

	if (!page)
		return 0;
	res += snprintf((page + res), PAGE_SIZE - res, "none\n");
	if (!list_empty(&tsn_shim_ops)) {
		list_for_each_entry(ops, &tsn_shim_ops, head) {
			res += snprintf((page + res), PAGE_SIZE - res, "%s\n",
					ops->shim_name);
		}
	}
	return res;
}



void tsn_lock_init(struct tsn_link *link)
{
	spin_lock_init(&link->tlock);
	link->lflags = 0;
	raw_spin_lock_init(&link->llock);
}


struct tsn_link *tsn_create_and_add_link(struct tsn_nic *nic)
{
	u64 sid = 0;
	struct tsn_link *link = kzalloc(sizeof(*link), GFP_KERNEL);

	if (!link)
		return NULL;
	if (!nic) {
		kfree(link);
		return NULL;
	}

	tsn_lock_init(link);
	tsn_link_off(link);
	tsn_lb_disable(link);
	do {
		sid = get_random_u32();
		sid |= get_random_u32() << 31;
	} while (tsn_find_by_stream_id(sid));
	link->stream_id = sid;

	/* There's a slim chance that we actually hit on the first frame
	 * of data, but if we do, remote seqnr is most likely 0. If this
	 * is not up to par,, fix in rx_handler
	 */
	link->last_seqnr = 0xff;

	/* class B audio 48kHz sampling, S16LE, 2ch and IEC61883-6 CIP
	 * header
	 */
	link->max_payload_size = 48;
	link->shim_header_size =  8;

	/* Default VLAN ID is SR_PVID (2) unless otherwise supplied from
	 * MSRP, PCP is default 3 for class A, 2 for Class B (See IEEE
	 * 802.1Q-2011, table 6-6)
	 */
	link->vlan_id = 0x3; //0x2;
	link->class = SR_CLASS_A; //SR_CLASS_B;

	link->buffer_size = 16536;
	/* default: talker since listener isn't implemented yet. */
	link->estype_talker = 1;

	link->nic = nic;

	link->ts_net_ns = ktime_to_ns(ktime_get());
	link->ts_delta_ns = 250000; /* initial class i B */
	link->ts_exp_alpha = 13107;	/* ~80% */

	/* Add the newly created link to the hashmap of all active links.
	 *
	 * test if sid is present in hashmap already (barf on that)
	 */

	tsn_list_lock(&tlist);
	hash_add(tlinks, &link->node, link->stream_id);
	tsn_list_unlock(&tlist);
	pr_info("%s: added link with stream_id: %llu\n",
		__func__, link->stream_id);

	return link;
}




ssize_t tsn_get_stream_ids(char *page, ssize_t len)
{
	struct tsn_link *link;
	struct hlist_node *tmp;
	char *buffer = page;
	int bkt;

	if (!page)
		return 0;

	if (hash_empty(tlinks))
		return sprintf(buffer, "no links registered\n");

	hash_for_each_safe(tlinks, bkt, tmp, link, node)
		buffer += sprintf(buffer, "%llu\n", link->stream_id);

	return (buffer - page);
}





struct tsn_link *tsn_find_by_stream_id(u64 sid)
{
	struct tsn_link *link;

	if (hash_empty(tlinks))
		return 0;

	hash_for_each_possible(tlinks, link, node, sid) {
		if (link->stream_id == sid)
			return link;
	}

	return NULL;
}




void tsn_remove_link(struct tsn_link *link)
{
	if (!link)
		return;
	tsn_net_close(link);
	tsn_list_lock(&tlist);
	hash_del(&link->node);
	if (link->ops) {
		link->ops->media_close(link);
		link->ops = NULL;
	}

	tsn_list_unlock(&tlist);
}




void tsn_remove_and_free_link(struct tsn_link *link)
{
	if (!link)
		return;
	tsn_remove_link(link);
	kfree(link);
}




void tsn_readd_link(struct tsn_link *link, u64 newkey)
{
	if (!link)
		return;
	tsn_lock(link);
	if (hash_hashed(&link->node)) {
		pr_info("%s: updating link with stream_id %llu -> %llu\n",
			__func__, link->stream_id, newkey);
		tsn_remove_link(link);
	}

	link->stream_id = newkey;
	tsn_unlock(link);

	hash_add(tlinks, &link->node, link->stream_id);
}



static int _tsn_capable_nic(struct net_device *netdev, struct tsn_nic *nic)
{
	if (!nic || !netdev || !netdev->netdev_ops ||
	    !netdev->netdev_ops->ndo_tsn_capable)
		return -EINVAL;

	if (netdev->netdev_ops->ndo_tsn_capable(netdev) > 0)
		nic->capable = 1;

	pr_info("%s: ndo_tsn_capable() present, got %s\n", __func__, nic->capable ? "Capable" : "Not capable");
	return 0;
}



static int tsn_nic_probe(void)
{
	struct net *net;
	struct net_device *netdev;
	struct tsn_nic *nic;

	net = &init_net;
	rcu_read_lock();
	for_each_netdev_rcu(net, netdev) {
		pr_info("Found %s, alias %s on irq %d\n",
			netdev->name,
			netdev->ifalias->ifalias,
			netdev->irq);
		pr_info("MAC: %pM", netdev->dev_addr);
		if (netdev->tx_queue_len)
			pr_info("Tx queue length: %u\n", netdev->tx_queue_len);

		/* GFP_KERNEL is preferred, but we are in rcu_read_lock()-region */
		nic = kzalloc(sizeof(*nic), GFP_ATOMIC);

		if (!nic) {
			pr_err("Could not allocate memory for tsn_nic!\n");
			return -ENOMEM;
		}
		nic->dev = netdev;
		nic->txq = netdev->num_tx_queues;
		nic->name = netdev->name;
		nic->tsn_list = &tlist;
		nic->dma_size = 1048576;

		/* This is the default values for A and B specified in 802.1Q */
		nic->pcp_a = 3;
		nic->pcp_b = 2;

		_tsn_capable_nic(netdev, nic);

		/* if not capable and we are not in debug-mode, drop nic
		 * and continue
		 */
		if (!nic->capable && !in_debug) {
			pr_info("Invalid capabilities for NIC (%s), dropping from TSN list\n",
				netdev->name);
			kfree(nic);
			continue;
		}

		INIT_LIST_HEAD(&nic->list);
		tsn_list_lock(&tlist);
		list_add_tail(&nic->list, &tlist.head);
		tlist.num_avail++;
		tsn_list_unlock(&tlist);
	}
	rcu_read_unlock();

	return 0;
}




static void tsn_free_nic_list(struct tsn_list *list)
{
	struct tsn_nic *tmp, *next;

	tsn_list_lock(list);
	list_for_each_entry_safe(tmp, next, &list->head, list) {
		pr_info("Dropping %s from list\n", tmp->dev->name);
		list_del(&tmp->list);
		tmp->dev = NULL;
		kfree(tmp);
	}
	tsn_list_unlock(list);
}




static void tsn_remove_all_links(void)
{
	int bkt;
	struct tsn_link *link;
	struct hlist_node *tmp;

	hash_for_each_safe(tlinks, bkt, tmp, link, node) {
		pr_info("%s removing a link\n", __func__);
		if (!tsn_teardown_link(link)) {
			tsn_lock(link);

			tsn_unlock(link);
		}
	}

	pr_info("%s: all links have been removed\n", __func__);
}




static int __init tsn_init_module(void)
{
	int ret = 0;

	INIT_LIST_HEAD(&tlist.head);
	spin_lock_init(&tlist.lock);

	atomic_set(&tlist.running, 0);
	tlist.period_ns =  1000000;

	/* Find all NICs, attach a rx-handler for sniffing out TSN
	 * traffic on *all* of them.
	 */
	tlist.num_avail = 0;
	ret = tsn_nic_probe();
	if (ret < 0) {
		pr_err("%s: somethign went awry whilst probing for NICs, aborting\n",
		       __func__);
		goto out;
	}

	if (!tlist.num_avail) {
		pr_err("%s: No capable NIC found. Perhaps load with in_debug=1 ?\n",
		       __func__);
		ret = -EINVAL;
		goto out;
	}

	/* register Rx-callbacks for all (valid) NICs */
	ret = tsn_net_add_rx(&tlist);
	if (ret < 0) {
		pr_err("%s: Could add Rx-handler, aborting\n", __func__);
		goto error_rx_out;
	}

	/* init DMA regions etc */
	ret = tsn_net_prepare_tx(&tlist);
	if (ret < 0) {
		pr_err("%s: could not prepare Tx, aborting\n", __func__);
		goto error_tx_out;
	}

	/* init hashtable */
	hash_init(tlinks);

	/* init configfs */
	ret = tsn_configfs_init(&tlist);
	if (ret < 0) {
		pr_err("%s: Could not initialize configfs properly (%d), aborting\n",
			__func__, ret);
		goto error_cfs_out;
	}
	pr_info("%s: configfs created\n", __func__);

	/* creating worker and thread.
	 * hrtimer_init will wake tsn_worker when timer is ready
	 *
	 * FIXME: make it possible to pin both timer and worker to a
	 * specific core.
	 */
	tlist.should_run = 0;
	ret = tsn_worker_init(&tlist);
	if (ret < 0) {
		pr_err("Failed to create tsn_worker!\n");
		goto error_hrt_out;
	}

	ret = tsn_hrtimer_init(&tlist);
	if (ret < 0) {
		pr_err("%s: could not init hrtimer properly, aborting\n",
			__func__);
		goto error_thread_out;
	}

	if (tsn_enabled == false) {
		hrtimer_cancel(&tlist.tsn_timer);
	}

	pr_info("TSN subsystem init OK\n");
	return 0;

error_thread_out:
	tsn_worker_exit(&tlist);
error_hrt_out:
	tsn_remove_all_links();
	tsn_configfs_exit(&tlist);
error_cfs_out:
	tsn_net_disable_tx(&tlist);
error_tx_out:
	tsn_net_remove_rx(&tlist);
error_rx_out:
	tsn_free_nic_list(&tlist);
out:
	return ret;
}



static void __exit tsn_exit_module(void)
{
	pr_warn("removing module TSN\n");

	tsn_worker_exit(&tlist);

	tsn_hrtimer_exit(&tlist);

	tsn_remove_all_links();
	tsn_configfs_exit(&tlist);

	/* Unregister Rx-handlers if set */
	tsn_net_remove_rx(&tlist);

	tsn_net_disable_tx(&tlist);

	tsn_free_nic_list(&tlist);

	pr_warn("TSN exit\n");
}

static int tsn_enabled_set(const char *val, const struct kernel_param *kp) {
	int ret;

	ret = param_set_bool(val, kp);
	if (ret < 0)
		return ret;

	if (tsn_enabled == false) {
		hrtimer_cancel(&tlist.tsn_timer);
		pr_debug("%s cancel hrtimer", __func__);
	} else {
		hrtimer_restart(&tlist.tsn_timer);
		pr_debug("%s restart hrtimer", __func__);
	}
	return 0;
}


static struct kernel_param_ops tsn_enabled_param_ops = {
	.set =		tsn_enabled_set,
	.get =		param_get_bool,
};


module_param(in_debug, int, S_IRUGO);
module_param(on_cpu, int, S_IRUGO);
module_param_cb(enabled, &tsn_enabled_param_ops, &tsn_enabled, 0644);
module_init(tsn_init_module);
module_exit(tsn_exit_module);
MODULE_AUTHOR("Henrik Austad");
MODULE_LICENSE("GPL");
