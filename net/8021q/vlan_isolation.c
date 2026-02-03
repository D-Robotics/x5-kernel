#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include "vlan.h"
#include <linux/module.h>
#include <linux/ip.h>
#include <net/route.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>

static unsigned int
vlan_strict_hook(void *priv, struct sk_buff *skb,
				 const struct nf_hook_state *state)
{
	struct iphdr *iph;
	struct net_device *dev = skb->dev;
	__be32 src_ip, dst_ip;

	/* Non-VLAN devices or non-IP protocols are directly permitted. */
	if (!dev || !is_vlan_dev(dev))
		return NF_ACCEPT;

	if (skb->protocol != htons(ETH_P_IP))
		return NF_ACCEPT;

	if (!pskb_may_pull(skb, sizeof(struct iphdr)))
		goto drop;

	iph = ip_hdr(skb);

	if (unlikely(iph->ihl < 5 || !pskb_may_pull(skb, iph->ihl * 4)))
		goto drop;

	iph = ip_hdr(skb);
	src_ip = iph->saddr;
	dst_ip = iph->daddr;

	/* Network Segment Verification: Subnet Mask Logic */
	if (unlikely((src_ip & htonl(0xFFFFFF00)) != (dst_ip & htonl(0xFFFFFF00)))) {
		// pr_err("VLAN Isolation: %pI4 -> %pI4 blocked\n", &src_ip, &dst_ip);
		goto drop;
	}

	return NF_ACCEPT;

drop:
	if (dev)
		dev->stats.rx_dropped++;
	return NF_DROP;
}

static struct nf_hook_ops vlan_nf_hook_ops __read_mostly = {
	.hook		= vlan_strict_hook,
	.hooknum	= NF_INET_PRE_ROUTING,
	.pf			= NFPROTO_IPV4,
	.priority	= NF_IP_PRI_FIRST,
};

static int __init vlan_isolation_init(void)
{
	int ret;

	ret = nf_register_net_hook(&init_net, &vlan_nf_hook_ops);
	if (ret < 0) {
		pr_err("Failed to register VLAN isolation hook\n");
		return ret;
	}

	pr_info("VLAN isolation module loaded\n");
	return 0;
}

static void __exit vlan_isolation_exit(void)
{
	nf_unregister_net_hook(&init_net, &vlan_nf_hook_ops);
	pr_info("VLAN isolation module unloaded\n");
}

module_init(vlan_isolation_init);
module_exit(vlan_isolation_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("developer@d-robotics.cc");
MODULE_DESCRIPTION("VLAN Isolation Module");
MODULE_VERSION("1.0");
