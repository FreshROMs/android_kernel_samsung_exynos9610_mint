#ifndef __NET_DROPDUMP_H
#define __NET_DROPDUMP_H

#include <linux/types.h>
#include <linux/inet.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
	
static inline struct list_head *dropdump_ptype_head(const struct packet_type *pt)
{
#ifdef CONFIG_NET_SUPPORT_DROPDUMP
	extern struct list_head ptype_log;

	if (unlikely(pt->type == htons(ETH_P_LOG)))
		return &ptype_log;
#endif
	return NULL;
}

/* packet drop-point ID */

enum dropdump_drop_id {
	/* netfilter */
	NET_DROPDUMP_NETFILTER_DROP = 0,
	NET_DROPDUMP_NETFILTER_STOLEN,

	/* IPSTATS_MIB_ */
	NET_DROPDUMP_IPSTATS_MIB_INHDRERRORS = 10,
	NET_DROPDUMP_IPSTATS_MIB_INTOOBIGERRORS,
	NET_DROPDUMP_IPSTATS_MIB_INNOROUTES,
	NET_DROPDUMP_IPSTATS_MIB_INADDRERRORS,
	NET_DROPDUMP_IPSTATS_MIB_INUNKNOWNPROTOS,
	NET_DROPDUMP_IPSTATS_MIB_INTRUNCATEDPKTS,
	NET_DROPDUMP_IPSTATS_MIB_INDISCARDS,
	NET_DROPDUMP_IPSTATS_MIB_CSUMERRORS,
	NET_DROPDUMP_IPSTATS_MIB_OUTDISCARDS,
	NET_DROPDUMP_IPSTATS_MIB_OUTNOROUTES,
	NET_DROPDUMP_IPSTATS_MIB_FRAGFAILS,

	/* TCP_MIB_ */
	NET_DROPDUMP_TCP_MIB_INERRS = 50,
	NET_DROPDUMP_TCP_MIB_CSUMERRORS,

	/* UDP_MIB_*/
	NET_DROPDUMP_UDP_MIB_NOPORTS = 70,
	NET_DROPDUMP_UDP_MIB_INERRORS,
	NET_DROPDUMP_UDP_MIB_RCVBUFERRORS,
	NET_DROPDUMP_UDP_MIB_CSUMERRORS,
	NET_DROPDUMP_UDP_MIB_IGNOREDMULTI,

	/* custom IDs */
	NET_DROPDUMP_CUSTOM_BEGIN = 100,

	NET_DROPDUMP_CUSTOM_END = 255
};

#ifdef CONFIG_NET_SUPPORT_DROPDUMP
extern int netdev_support_dropdump;

#define DROPDUMP_QUEUE_SKB(skb, id) \
	dropdump_queue_precondition(skb, id)

extern void dropdump_queue_precondition(struct sk_buff *skb, enum dropdump_drop_id id);
extern void dropdump_queue_skb(struct sk_buff *skb, u8 pkt_type, enum dropdump_drop_id id);

#else
#define DROPDUMP_QUEUE_SKB(skb, id) {}
#define dropdump_queue_precondition(skb, id) {} 
#define dropdump_queue_skb(skb, pkt_type, id) {}

#endif

#endif //__NET_DROPDUMP_H
