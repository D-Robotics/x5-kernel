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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/configfs.h>
#include <linux/netdevice.h>
#include <linux/rtmutex.h>
#include <linux/tsn.h>
#include "tsn_internal.h"


static inline struct tsn_link *to_tsn_link(struct config_item *item)
{
	/* this line causes checkpatch to WARN. making checkpatch happy,
	 * makes code messy..
	 */
	return item ? container_of(to_config_group(item), struct tsn_link, group) : NULL;
}


static inline struct tsn_nic *to_tsn_nic(struct config_group *group)
{
	return group ? container_of(group, struct tsn_nic, group) : NULL;
}

static inline struct tsn_nic *item_to_tsn_nic(struct config_item *item)
{
	return item ? container_of(to_config_group(item), struct tsn_nic, group) : NULL;
}


static ssize_t _tsn_max_payload_size_show(struct config_item *item,
					  char *page)
{
	struct tsn_link *link = to_tsn_link(item);

	if (!link)
		return -EINVAL;
	return sprintf(page, "%u\n", (u32)link->max_payload_size);
}

static ssize_t _tsn_max_payload_size_store(struct config_item *item, const char *page, size_t count)
{
	struct tsn_link *link = to_tsn_link(item);
	u16 mpl_size = 0;
	int ret = 0;

	if (!link)
		return -EINVAL;
	if (tsn_link_is_on(link)) {
		pr_err("ERROR: Cannot change Payload size on link\n");
		return -EINVAL;
	}
	ret = kstrtou16(page, 0, &mpl_size);
	if (ret)
		return ret;

	/* 802.1BA-2011 6.4 payload must be <1500 octets (excluding
	 * headers, tags etc) However, this is not directly mappable to
	 * how some hw handles things, so to be conservative, we
	 * restrict it down to [26..1485]
	 *
	 * This is also the _payload_ size, which does not include the
	 * AVTPDU header. This is an upper limit to how much raw data
	 * the shim can transport in each frame.
	 */
	if (!tsnh_payload_size_valid(mpl_size, link->shim_header_size)) {
		pr_err("%s: payload (%u) should be [26..1480] octets.\n",
		       __func__, (u32)mpl_size);
		return -EINVAL;
	}
	link->max_payload_size = mpl_size;
	return count;
}


static ssize_t _tsn_shim_header_size_show(struct config_item *item,
					  char *page)
{
	struct tsn_link *link = to_tsn_link(item);

	if (!link)
		return -EINVAL;
	return sprintf(page, "%u\n", (u32)link->shim_header_size);
}

static ssize_t _tsn_shim_header_size_store(struct config_item *item,
					   const char *page, size_t count)
{
	struct tsn_link *link = to_tsn_link(item);
	u16 hdr_size = 0;
	int ret = 0;

	if (!link)
		return -EINVAL;
	if (tsn_link_is_on(link)) {
		pr_err("ERROR: Cannot change shim-header size on link\n");
		return -EINVAL;
	}

	ret = kstrtou16(page, 0, &hdr_size);
	if (ret)
		return ret;

	if (!tsnh_payload_size_valid(link->max_payload_size, hdr_size))
		return -EINVAL;

	link->shim_header_size = hdr_size;
	return count;
}

static ssize_t _tsn_stream_id_show(struct config_item *item, char *page)
{
	struct tsn_link *link = to_tsn_link(item);

	if (!link)
		return -EINVAL;
	return sprintf(page, "%llu\n", link->stream_id);
}


static ssize_t _tsn_stream_id_store(struct config_item *item,
				    const char *page, size_t count)
{
	struct tsn_link *link = to_tsn_link(item);
	u64 sid;
	int ret = 0;

	if (!link)
		return -EINVAL;
	if (tsn_link_is_on(link)) {
		pr_err("ERROR: Cannot change StreamID on link\n");
		return -EINVAL;
	}
	ret = kstrtou64(page, 0, &sid);
	if (ret)
		return ret;

	if (sid == link->stream_id)
		return count;

	if (tsn_find_by_stream_id(sid)) {
		pr_warn("Cannot set sid to %llu - exists\n", sid);
		return -EEXIST;
	}
	if (sid != link->stream_id)
		tsn_readd_link(link, sid);
	return count;
}

static ssize_t _tsn_buffer_size_show(struct config_item *item, char *page)
{
	struct tsn_link *link = to_tsn_link(item);

	if (!link)
		return -EINVAL;
	return sprintf(page, "%zu\n", link->buffer_size);
}

static ssize_t _tsn_buffer_size_store(struct config_item *item,
				      const char *page, size_t count)
{
	struct tsn_link *link = to_tsn_link(item);
	u32 tmp;
	int ret = 0;

	if (!link)
		return -EINVAL;
	if (tsn_link_is_on(link)) {
		pr_err("ERROR: Cannot change Buffer Size on link\n");
		return -EINVAL;
	}

	ret = kstrtou32(page, 0, &tmp);
	/* only allow buffers !0 and smaller than 8MB for now */
	if (!ret && tmp) {
		pr_info("%s: update buffer_size from %zu to %u\n",
			__func__, link->buffer_size, tmp);
		link->buffer_size = (size_t)tmp;
		return count;
	}
	return -EINVAL;
}

static ssize_t _tsn_class_show(struct config_item *item, char *page)
{
	struct tsn_link *link = to_tsn_link(item);

	if (!link)
		return -EINVAL;
	return sprintf(page, "%s\n", (link->class == SR_CLASS_A ? "A" : "B"));
}


static ssize_t _tsn_class_store(struct config_item *item,
				const char *page, size_t count)
{
	char class[2] = { 0 };
	struct tsn_link *link = to_tsn_link(item);

	if (!link)
		return -EINVAL;
	if (tsn_link_is_on(link)) {
		pr_err("ERROR: Cannot change Class-type on link\n");
		return -EINVAL;
	}
	if (strncpy(class, page, 1)) {
		if (strcmp(class, "a") == 0 || strcmp(class, "A") == 0)
			link->class = SR_CLASS_A;
		else if (strcmp(class, "b") == 0 || strcmp(class, "B") == 0)
			link->class = SR_CLASS_B;
		return count;
	}

	pr_err("%s: Could not copy new class into buffer\n", __func__);
	return -EINVAL;
}


static ssize_t _tsn_vlan_id_show(struct config_item *item, char *page)
{
	struct tsn_link *link = to_tsn_link(item);

	if (!link)
		return -EINVAL;
	return sprintf(page, "%u\n", link->vlan_id);
}


static ssize_t _tsn_vlan_id_store(struct config_item *item,
				  const char *page, size_t count)
{
	struct tsn_link *link = to_tsn_link(item);
	u16 vlan_id;
	int ret = 0;

	if (!link)
		return -EINVAL;
	if (tsn_link_is_on(link)) {
		pr_err("ERROR: Cannot change VLAN-ID on link\n");
		return -EINVAL;
	}
	ret = kstrtou16(page, 0, &vlan_id);
	if (ret)
		return ret;
	if (vlan_id > 0xfff)
		return -EINVAL;
	link->vlan_id = vlan_id & 0xfff;
	return count;
}




static ssize_t _tsn_end_station_show(struct config_item *item, char *page)
{
	struct tsn_link *link = to_tsn_link(item);

	if (!link)
		return -EINVAL;
	return sprintf(page, "%s\n",
		      (link->estype_talker ? "Talker" : "Listener"));
}


static ssize_t _tsn_end_station_store(struct config_item *item,
				      const char *page, size_t count)
{
	struct tsn_link *link = to_tsn_link(item);
	char estype[9] = {0};

	if (!link)
		return -EINVAL;
	if (tsn_link_is_on(link)) {
		pr_err("ERROR: Cannot change End-station type on link.\n");
		return -EINVAL;
	}
	if (strncpy(estype, page, 8)) {
		if (strncmp(estype, "Talker", 6) == 0 ||
		    strncmp(estype, "talker", 6) == 0) {
			link->estype_talker = 1;
			return count;
		} else if (strncmp(estype, "Listener", 8) == 0 ||
			   strncmp(estype, "listener", 8) == 0) {
			link->estype_talker = 0;
			return count;
		}
	}
	return -EINVAL;
}




static ssize_t _tsn_shim_show(struct config_item *item, char *page)
{
	struct tsn_link *link = to_tsn_link(item);
	if (!link)
		return -EINVAL;
	return sprintf(page, "%s\n", tsn_shim_get_active(link));
}


static ssize_t _tsn_shim_store(struct config_item *item,
			const char *page, size_t count)
{
	size_t len;
	ssize_t ret;
	char shim_name[SHIM_NAME_SIZE + 1] = { 0 };
	struct tsn_shim_ops *shim_ops = NULL;
	struct tsn_link *link = to_tsn_link(item);

	if (!link)
		return -EINVAL;
	if (tsn_link_is_on(link)) {
		pr_err("TSN ERROR: cannot change shim on link (is active)\n");
		return -EINVAL;
	}

	strncpy(shim_name, page, SHIM_NAME_SIZE);
	len = strlen(shim_name);
	while (len-- > 0) {
		if (shim_name[len] == '\n')
			shim_name[len] = 0x00;
	}

	/* the only shim we allow to set shim_ops to NULL is 'off' */
	if (strncmp(shim_name, "off", 3) != 0) {
		shim_ops = tsn_shim_find_by_name(shim_name);
		if (!shim_ops) {
			pr_info("TSN ERROR: could not enable desired shim, %s is not available\n",
				shim_name);
			return -EINVAL;
		}
	}

	ret = tsn_set_shim_ops(link, shim_ops);
	if (ret != 0) {
		pr_err("TSN ERROR: Could not set shim-ops for link - %zd\n", ret);
		return ret;
	}
	pr_info("TSN: Set new shim_ops (%s)\n", shim_name);
	return count;
}





static ssize_t _tsn_enabled_show(struct config_item *item, char *page)
{
	struct tsn_link *link = to_tsn_link(item);

	if (!link)
		return -EINVAL;
	return sprintf(page, "%s\n", tsn_link_is_on(link) ? "on" : "off");
}



static ssize_t _tsn_enabled_store(struct config_item *item,
				  const char *page, size_t count)
{
	struct tsn_link *link = to_tsn_link(item);
	char link_state[8] = {0};
	size_t len;
	int ret = 0;

	if (!link)
		return -EINVAL;

	strncpy(link_state, page, 7);
	len = strlen(link_state);
	while (len-- > 0) {
		if (link_state[len] == '\n')
			link_state[len] = 0x00;
	}

	/* only allowed state is off */
	if (tsn_link_is_on(link) || tsn_link_is_err(link)) {
		if (strncmp(link_state, "off", 3) != 0) {
			pr_err("TSN ERROR: Invalid link_state for active link (%s), ignoring\n", link_state);
			return -EINVAL;
		}
		tsn_teardown_link(link);
		return count;
	}
	else if (strncmp(link_state, "on", 3) == 0) {
		ret = tsn_prepare_link(link);
		if (ret != 0) {
			pr_err("TSN ERROR: Preparing link failed - %d\n", ret);
			return ret;
		}
		return count;
	}
	return -EINVAL;
}




static ssize_t _tsn_remote_mac_show(struct config_item *item, char *page)
{
	struct tsn_link *link = to_tsn_link(item);

	if (!link)
		return -EINVAL;
	return sprintf(page, "%pM\n", link->remote_mac);
}





static ssize_t _tsn_remote_mac_store(struct config_item *item,
				     const char *page, size_t count)
{
	struct tsn_link *link = to_tsn_link(item);
	unsigned char mac[7] = {0};
	int ret = 0;

	if (!link)
		return -EINVAL;
	if (tsn_link_is_on(link)) {
		pr_err("ERROR: Cannot change Remote MAC on link.\n");
		return -EINVAL;
	}
	ret = sscanf(page, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
		     &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
	if (ret > 0) {
		pr_info("Got MAC (%pM), copying to storage\n", &mac);
		memcpy(link->remote_mac, mac, 6);
		return count;
	}
	return -EINVAL;
}




static ssize_t _tsn_local_mac_show(struct config_item *item, char *page)
{
	struct tsn_link *link = to_tsn_link(item);

	if (!link)
		return -EINVAL;
	return sprintf(page, "%pMq\n", link->nic->dev->perm_addr);
}


CONFIGFS_ATTR(_tsn_, max_payload_size);
CONFIGFS_ATTR(_tsn_, shim_header_size);
CONFIGFS_ATTR(_tsn_, stream_id);
CONFIGFS_ATTR(_tsn_, buffer_size);
CONFIGFS_ATTR(_tsn_, class);
CONFIGFS_ATTR(_tsn_, vlan_id);
CONFIGFS_ATTR(_tsn_, end_station);
CONFIGFS_ATTR(_tsn_, shim);
CONFIGFS_ATTR(_tsn_, enabled);
CONFIGFS_ATTR(_tsn_, remote_mac);
CONFIGFS_ATTR_RO(_tsn_, local_mac);


static struct configfs_attribute *tsn_tier2_attrs[] = {
	&_tsn_attr_max_payload_size,
	&_tsn_attr_shim_header_size,
	&_tsn_attr_stream_id,
	&_tsn_attr_buffer_size,
	&_tsn_attr_class,
	&_tsn_attr_vlan_id,
	&_tsn_attr_end_station,
	&_tsn_attr_shim,
	&_tsn_attr_enabled,
	&_tsn_attr_remote_mac,
	&_tsn_attr_local_mac,
	NULL,
};



static struct config_item_type group_tsn_tier2_type = {
	.ct_owner     = THIS_MODULE,
	.ct_attrs     = tsn_tier2_attrs,
	.ct_group_ops = NULL,
};




static struct config_group *group_tsn_1_make_group(struct config_group *group,
						   const char *name)
{
	struct tsn_nic *nic = to_tsn_nic(group);
	struct tsn_link *link = tsn_create_and_add_link(nic);

	if (!nic || !link)
		return ERR_PTR(-ENOMEM);

	config_group_init_type_name(&link->group, name, &group_tsn_tier2_type);

	return &link->group;
}




static void group_tsn_1_drop_group(struct config_group *group,
				   struct config_item *item)
{
	struct tsn_link *link = to_tsn_link(item);
	struct tsn_nic *nic = to_tsn_nic(group);

	if (link) {
		tsn_teardown_link(link);
		tsn_remove_and_free_link(link);
	}
	pr_info("Dropping %s from NIC: %s\n", item->ci_name, nic->name);
}




static ssize_t _tsn_pcp_a_show(struct config_item *item, char *page)
{
	struct tsn_nic *nic = item_to_tsn_nic(item);

	if (!nic)
		return -EINVAL;
	return sprintf(page, "0x%x\n", nic->pcp_a);
}



static ssize_t _tsn_pcp_a_store(struct config_item *item,
				const char *page, size_t count)
{
	struct tsn_nic *nic = item_to_tsn_nic(item);
	int ret = 0;
	u8 pcp;

	if (!nic)
		return -EINVAL;

	/* FIXME: need to check for *any* active links */

	ret = kstrtou8(page, 0, &pcp);
	if (ret)
		return ret;
	if (pcp > 0x7)
		return -EINVAL;
	nic->pcp_a = pcp & 0x7;
	return count;
}

static ssize_t _tsn_pcp_b_show(struct config_item *item, char *page)
{
	struct tsn_nic *nic = item_to_tsn_nic(item);

	if (!nic)
		return -EINVAL;
	return sprintf(page, "0x%x\n", nic->pcp_b);
}

static ssize_t _tsn_pcp_b_store(struct config_item *item,
				const char *page, size_t count)
{
	struct tsn_nic *nic = item_to_tsn_nic(item);
	int ret = 0;
	u8 pcp;

	if (!nic)
		return -EINVAL;

	/* FIXME: need to check for *any* active links */

	ret = kstrtou8(page, 0, &pcp);
	if (ret)
		return ret;
	if (pcp > 0x7)
		return -EINVAL;
	nic->pcp_b = pcp & 0x7;
	return count;
}

CONFIGFS_ATTR(_tsn_, pcp_a);
CONFIGFS_ATTR(_tsn_, pcp_b);

static struct configfs_attribute *tsn_tier1_attrs[] = {
	&_tsn_attr_pcp_a,
	&_tsn_attr_pcp_b,
	NULL,
};




static struct configfs_group_operations group_tsn_1_group_ops = {
	.make_group	= group_tsn_1_make_group,
	.drop_item      = group_tsn_1_drop_group,
};

static struct config_item_type group_tsn_tier1_type = {
	.ct_group_ops	= &group_tsn_1_group_ops,
	.ct_attrs	= tsn_tier1_attrs,
	.ct_owner	= THIS_MODULE,
};


static ssize_t _tsn_used_sids_show(struct config_item *item, char *page)
{
	return tsn_get_stream_ids(page, PAGE_SIZE);
}

static ssize_t _tsn_available_shims_show(struct config_item *item, char *page)
{
	return tsn_shim_export_probe_triggers(page);
}


static struct configfs_attribute tsn_used_sids = {
	.ca_owner = THIS_MODULE,
	.ca_name  = "stream_ids",
	.ca_mode  = S_IRUGO,
	.show     = _tsn_used_sids_show,
};

static struct configfs_attribute available_shims = {
	.ca_owner = THIS_MODULE,
	.ca_name  = "available_shims",
	.ca_mode  = S_IRUGO,
	.show     = _tsn_available_shims_show,
};



static struct configfs_attribute *group_tsn_attrs[] = {
	&tsn_used_sids,
	&available_shims,
	NULL,
};

static struct config_item_type group_tsn_tier0_type = {
	.ct_group_ops	= NULL,
	.ct_attrs	= group_tsn_attrs,
	.ct_owner	= THIS_MODULE,
};



int tsn_configfs_init(struct tsn_list *tlist)
{
	int ret = 0;
	struct tsn_nic *next;
	struct configfs_subsystem *subsys;

	if (!tlist || !tlist->num_avail)
		return -EINVAL;

	/* Tier-0 */
	subsys = &tlist->tsn_subsys;
	strncpy(subsys->su_group.cg_item.ci_namebuf, "tsn",
		CONFIGFS_ITEM_NAME_LEN);
	subsys->su_group.cg_item.ci_type = &group_tsn_tier0_type;

	config_group_init(&subsys->su_group);
	mutex_init(&subsys->su_mutex);

	/* Tier-1
	 * (tsn-capable NICs), automatic subgroups
	 */
	list_for_each_entry(next, &tlist->head, list) {
		config_group_init_type_name(&next->group, next->name,
					    &group_tsn_tier1_type);
		configfs_add_default_group(&next->group, &subsys->su_group);
	}

	/* This is the final step, once done, system is live, make sure
	 * init has completed properly
	 */
	ret = configfs_register_subsystem(subsys);
	if (ret) {
		pr_err("Trouble registering TSN ConfigFS subsystem\n");
		return ret;
	}

	pr_warn("configfs_init_module() OK\n");
	return 0;
}

void tsn_configfs_exit(struct tsn_list *tlist)
{
	if (!tlist)
		return;
	configfs_unregister_subsystem(&tlist->tsn_subsys);
	pr_warn("configfs_exit_module()\n");
}
