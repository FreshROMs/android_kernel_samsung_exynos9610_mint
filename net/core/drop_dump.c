/*
 * Monitoring code for network dropped packet alerts
 *
 * Copyright (C) 2018 SAMSUNG Electronics, Co,LTD
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/string.h>
#include <linux/inetdevice.h>
#include <linux/inet.h>
#include <linux/interrupt.h>
#include <linux/netpoll.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/net_dropdump.h>
#include <linux/percpu.h>
#include <linux/timer.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <net/netevent.h>

#include <trace/events/skb.h>
#include <trace/events/napi.h>

#include <asm/unaligned.h>

struct list_head ptype_log __read_mostly;

int netdev_support_dropdump = 1;
EXPORT_SYMBOL(netdev_support_dropdump);

static inline int deliver_skb(struct sk_buff *skb,
			      struct packet_type *pt_prev,
			      struct net_device *orig_dev)
{
	if (unlikely(skb_orphan_frags_rx(skb, GFP_ATOMIC)))
		return -ENOMEM;
	refcount_inc(&skb->users);
	return pt_prev->func(skb, skb->dev, pt_prev, orig_dev);
}

static inline void net_timestamp_set(struct sk_buff *skb)
{
	/* we always need timestamp for dropdump */
	__net_timestamp(skb);
}

static void dropdump_print_skb(struct sk_buff *skb)
{
	struct iphdr *ip4hdr = (struct iphdr *)skb_network_header(skb);

	if (ip4hdr->version == 4) {
		pr_info("dqn: ip: src:%pI4, dst:%pI4, raw:%*ph\n",
			&ip4hdr->saddr, &ip4hdr->daddr, 48, ip4hdr);
	} else if (ip4hdr->version == 6) {
		struct ipv6hdr *ip6hdr = (struct ipv6hdr *)ip4hdr;

		pr_info("dqn: ip: src:%pI6, dst:%pI6, raw:%*ph\n",
			&ip6hdr->saddr, &ip6hdr->daddr, 48, ip6hdr);
	}
}

void dropdump_queue_skb(struct sk_buff *skb, u8 pkt_type,
			enum dropdump_drop_id id)
{
	struct packet_type *ptype;
	struct packet_type *pt_prev = NULL;
	struct sk_buff *skb2 = NULL;
	struct list_head *ptype_list = &ptype_log;
	struct net_device *dev = NULL;
	struct net_device *old_dev;
	struct iphdr *iphdr;

	if (unlikely(!skb))
		return;

	old_dev = skb->dev;
	if (skb->dev)
		dev = skb->dev;
	else if (skb_dst(skb) && skb_dst(skb)->dev)
		dev = skb_dst(skb)->dev;
	else
		dev = init_net.loopback_dev;

	/* final check */
	if (!dev)
		return;

	rcu_read_lock_bh();
	skb->dev = dev;

	list_for_each_entry_rcu(ptype, ptype_list, list) {
		if (pt_prev) {
			deliver_skb(skb2, pt_prev, dev);
			pt_prev = ptype;
			continue;
		}

		/* need to clone skb, done only once */
		skb2 = skb_clone(skb, GFP_ATOMIC);
		if (!skb2)
			goto out_unlock;

		net_timestamp_set(skb2);

		/* skb->nh should be correctly
		 * set by sender, so that the second statement is
		 * just protection against buggy protocols.
		 */
		skb_reset_mac_header(skb2);

		if (skb_network_header(skb2) > skb_tail_pointer(skb2)) {
			net_crit_ratelimited("protocol %04x is buggy, dev %s\n",
					     ntohs(skb2->protocol),
					     dev->name);
			skb_reset_network_header(skb2);
		}

		if (pkt_type != PACKET_OUTGOING &&
		    skb2->data > skb_network_header(skb2)) {
			if (skb->transport_header - skb->network_header > 0)
				skb_push(skb2, skb_network_header_len(skb2));
			else
				skb_push(skb2, skb2->data -
						skb_network_header(skb2));
		}

		skb2->transport_header = skb2->network_header;
		skb2->pkt_type = pkt_type; /*PACKET_OUTGOING*/
		pt_prev = ptype;

		iphdr = (struct iphdr *)skb_network_header(skb2);
		if (iphdr->version == 4) {
			iphdr->ttl = (u8)id;
		} else if (iphdr->version == 6) {
			struct ipv6hdr *ip6hdr = (struct ipv6hdr *)iphdr;

			ip6hdr->hop_limit = (u8)id;
		} else {
			/* no IP packet */
		}
	}

out_unlock:
	if (pt_prev) {
		if (!skb_orphan_frags_rx(skb2, GFP_ATOMIC))
			pt_prev->func(skb2, skb->dev, pt_prev, skb->dev);
		else
			kfree_skb(skb2);
	}

	skb->dev = old_dev;
	rcu_read_unlock_bh();
}

void dropdump_queue_precondition(struct sk_buff *skb,
				 enum dropdump_drop_id id)
{
	u8 pkt_type = PACKET_HOST;

	if (unlikely(!skb))
		return;

	dropdump_print_skb(skb);

	switch (id) {
	case NET_DROPDUMP_IPSTATS_MIB_OUTDISCARDS:
	case NET_DROPDUMP_IPSTATS_MIB_OUTNOROUTES:
	case NET_DROPDUMP_IPSTATS_MIB_FRAGFAILS:
		pkt_type = PACKET_OUTGOING;
		break;
	default:
		break;
	}

	dropdump_queue_skb(skb, pkt_type, id);
}

static int __init init_net_drop_dump(void)
{
	INIT_LIST_HEAD(&ptype_log);

	return 0;
}

static void exit_net_drop_dump(void)
{
}

module_init(init_net_drop_dump);
module_exit(exit_net_drop_dump);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Samsung dropdump module");
