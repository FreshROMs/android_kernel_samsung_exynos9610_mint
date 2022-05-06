/******************************************************************************
 *
 * Copyright (c) 2012 - 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/

#ifndef SLSI_UTILS_H__
#define SLSI_UTILS_H__

#include <linux/version.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/skbuff.h>
#include <net/cfg80211.h>

#include "netif.h"
#include "fapi.h"

struct slsi_skb_cb {
	u32 sig_length;
	u32 data_length;
	u32 frame_format;
#ifdef CONFIG_SCSC_SMAPPER
	bool free_ma_unitdat;
	struct sk_buff *skb_addr;
#endif
	bool wakeup;
};
struct netdev_vif;

static inline struct slsi_skb_cb *slsi_skb_cb_get(struct sk_buff *skb)
{
	return (struct slsi_skb_cb *)skb->cb;
}

static inline struct slsi_skb_cb *slsi_skb_cb_init(struct sk_buff *skb)
{
	BUILD_BUG_ON(sizeof(struct slsi_skb_cb) > sizeof(skb->cb));

	memset(skb->cb, 0, sizeof(struct slsi_skb_cb));
	return slsi_skb_cb_get(skb);
}

#define fapi_alloc(mp_name, mp_id, mp_vif, mp_datalen) fapi_alloc_f(fapi_sig_size(mp_name), mp_datalen, mp_id, mp_vif, __FILE__, __LINE__)
#define fapi_get_buff(mp_skb, mp_name) (((struct fapi_signal *)(mp_skb)->data)->mp_name)
#define fapi_get_u16(mp_skb, mp_name) le16_to_cpu(((struct fapi_signal *)(mp_skb)->data)->mp_name)
#define fapi_get_u32(mp_skb, mp_name) le32_to_cpu(((struct fapi_signal *)(mp_skb)->data)->mp_name)
#define fapi_get_u64(mp_skb, mp_name) le64_to_cpu(((struct fapi_signal *)(mp_skb)->data)->mp_name)
#define fapi_set_u16(mp_skb, mp_name, mp_value) (((struct fapi_signal *)(mp_skb)->data)->mp_name = cpu_to_le16(mp_value))
#define fapi_set_u32(mp_skb, mp_name, mp_value) (((struct fapi_signal *)(mp_skb)->data)->mp_name = cpu_to_le32(mp_value))
#define fapi_get_s16(mp_skb, mp_name) ((s16)le16_to_cpu(((struct fapi_signal *)(mp_skb)->data)->mp_name))
#define fapi_get_s32(mp_skb, mp_name) ((s32)le32_to_cpu(((struct fapi_signal *)(mp_skb)->data)->mp_name))
#define fapi_set_s16(mp_skb, mp_name, mp_value) (((struct fapi_signal *)(mp_skb)->data)->mp_name = cpu_to_le16((u16)mp_value))
#define fapi_set_s32(mp_skb, mp_name, mp_value) (((struct fapi_signal *)(mp_skb)->data)->mp_name = cpu_to_le32((u32)mp_value))
#define fapi_set_memcpy(mp_skb, mp_name, mp_value) memcpy(((struct fapi_signal *)(mp_skb)->data)->mp_name, mp_value, sizeof(((struct fapi_signal *)(mp_skb)->data)->mp_name))
#define fapi_set_memset(mp_skb, mp_name, mp_value) memset(((struct fapi_signal *)(mp_skb)->data)->mp_name, mp_value, sizeof(((struct fapi_signal *)(mp_skb)->data)->mp_name))

/* Helper to get and set high/low 16 bits from u32 signals */
#define fapi_get_high16_u32(mp_skb, mp_name) ((fapi_get_u32((mp_skb), mp_name) & 0xffff0000) >> 16)
#define fapi_set_high16_u32(mp_skb, mp_name, mp_value) fapi_set_u32((mp_skb), mp_name, (fapi_get_u32((mp_skb), mp_name) & 0xffff) | ((mp_value) << 16))
#define fapi_get_low16_u32(mp_skb, mp_name) (fapi_get_u32((mp_skb), mp_name) & 0xffff)
#define fapi_set_low16_u32(mp_skb, mp_name, mp_value) fapi_set_u32((mp_skb), mp_name, (fapi_get_u32((mp_skb), mp_name) & 0xffff0000) | (mp_value))

#define fapi_get_siglen(mp_skb) (slsi_skb_cb_get(mp_skb)->sig_length)
#define fapi_get_datalen(mp_skb) (slsi_skb_cb_get(mp_skb)->data_length - slsi_skb_cb_get(mp_skb)->sig_length)
#define fapi_get_data(mp_skb) (mp_skb->data + fapi_get_siglen(mp_skb))
#define fapi_get_vif(mp_skb) le16_to_cpu(((struct fapi_vif_signal_header *)(mp_skb)->data)->vif)

/* Helper to get the struct ieee80211_mgmt from the data */
#define fapi_get_mgmt(mp_skb) ((struct ieee80211_mgmt *)fapi_get_data(mp_skb))
#define fapi_get_mgmtlen(mp_skb) fapi_get_datalen(mp_skb)

static inline struct sk_buff *fapi_alloc_f(size_t sig_size, size_t data_size, u16 id, u16 vif, const char *file, int line)
{
	struct sk_buff                *skb = NULL;
	struct fapi_vif_signal_header *header;

	if (WARN_ON(in_interrupt()))
		return NULL;
	skb = alloc_skb(sig_size + data_size, GFP_KERNEL);
	WARN_ON(sig_size < sizeof(struct fapi_signal_header));
	if (WARN_ON(!skb))
		return NULL;

	slsi_skb_cb_init(skb)->sig_length = sig_size;
	slsi_skb_cb_get(skb)->data_length = sig_size;

	header = (struct fapi_vif_signal_header *)skb_put(skb, sig_size);
	header->id = cpu_to_le16(id);
	header->receiver_pid = 0;
	header->sender_pid = 0;
	header->fw_reference = 0;
	header->vif = vif;
	return skb;
}

static inline u8 *fapi_append_data(struct sk_buff *skb, const u8 *data, size_t data_len)
{
	u8 *p;

	if (WARN_ON(skb_tailroom(skb) < data_len))
		return NULL;

	p = skb_put(skb, data_len);
	slsi_skb_cb_get(skb)->data_length += data_len;
	if (data)
		memcpy(p, data, data_len);
	return p;
}

static inline u8 *fapi_append_data_u32(struct sk_buff *skb, const u32 data)
{
	__le32 val = cpu_to_le32(data);

	return fapi_append_data(skb, (u8 *)&val, sizeof(val));
}

static inline u8 *fapi_append_data_u16(struct sk_buff *skb, const u16 data)
{
	__le16 val = cpu_to_le16(data);

	return fapi_append_data(skb, (u8 *)&val, sizeof(val));
}

static inline u8 *fapi_append_data_u8(struct sk_buff *skb, const u8 data)
{
	u8 val = data;

	return fapi_append_data(skb, (u8 *)&val, sizeof(val));
}

#define fapi_append_data_bool(skb, data) fapi_append_data_u16(skb, data)

static inline u32  slsi_convert_tlv_data_to_value(u8 *data, u16 length)
{
	u32 value = 0;
	int i;

	if (length > 4)
		return 0;
	for (i = 0; i < length; i++)
		value |= ((u32)data[i]) << i * 8;

	return value;
}

static inline void  slsi_convert_tlv_to_64bit_value(u8 *data, u16 length, u32 *lower, u32 *higher)
{
	int i;

	*lower = 0;
	*higher = 0;
	if (length > 8)
		return;
	for (i = 0; i < 4 && i < length; i++)
		*lower |= ((u32)data[i]) << i * 8;
	for (i = 4; i < 8 && i < length; i++)
		*higher |= ((u32)data[i]) << (i - 4) * 8;
}

#ifdef __cplusplus
extern "C" {
#endif

#define SLSI_ETHER_COPY(dst, src)	ether_addr_copy((dst), (src))
#define SLSI_ETHER_EQUAL(mac1, mac2)	ether_addr_equal((mac1), (mac2))

extern uint slsi_sg_host_align_mask;
#define SLSI_HIP_FH_SIG_PREAMBLE_LEN 4
#define SLSI_SKB_GET_ALIGNMENT_OFFSET(skb) (0)

/* Get the Compiler to ignore Unused parameters */
#define SLSI_UNUSED_PARAMETER(x) ((void)(x))

/* Helper ERROR Macros */
#define SLSI_ECR(func) \
	do { \
		int _err = (func); \
		if (_err != 0) { \
			SLSI_ERR_NODEV("e=%d\n", _err); \
			return _err; \
		} \
	} while (0)

#define SLSI_EC(func) \
	do { \
		int _err = (func); \
		if (_err != 0) { \
			SLSI_ERR_NODEV("e=%d\n", _err); \
			return; \
		} \
	} while (0)

#define SLSI_EC_GOTO(func, err, label) \
	do { \
		(err) = func; \
		if ((err) != 0) { \
			WARN_ON(1); \
			SLSI_ERR(sdev, "fail at line:%d\n", __LINE__); \
			goto label; \
		} \
	} while (0)

/*------------------------------------------------------------------*/
/* Endian conversion. */
/*------------------------------------------------------------------*/
#define SLSI_BUFF_LE_TO_U16(ptr)        (((u16)((u8 *)(ptr))[0]) | ((u16)((u8 *)(ptr))[1]) << 8)
#define SLSI_U16_TO_BUFF_LE(uint, ptr) \
	do { \
		u32 local_uint_tmp = (uint); \
		((u8 *)(ptr))[0] = ((u8)((local_uint_tmp & 0x00FF))); \
		((u8 *)(ptr))[1] = ((u8)(local_uint_tmp >> 8)); \
	} while (0)

#define SLSI_U32_TO_BUFF_LE(uint, ptr) ((*(u32 *)ptr) = cpu_to_le32(uint))

#define SLSI_BUFF_LE_TO_U16_P(output, input) \
	do { \
		(output) = (u16)((((u16)(input)[1]) << 8) | ((u16)(input)[0])); \
		(input) += 2; \
	} while (0)

#define SLSI_BUFF_LE_TO_U32_P(output, input) \
	do { \
		(output) = le32_to_cpu(*(u32 *)input); \
		(input) += 4; \
	} while (0)

#define SLSI_U16_TO_BUFF_LE_P(output, input) \
	do { \
		(output)[0] = ((u8)((input) & 0x00FF));  \
		(output)[1] = ((u8)((input) >> 8)); \
		(output) += 2; \
	} while (0)

#define SLSI_U32_TO_BUFF_LE_P(output, input) \
	do { \
		(*(u32 *)output) = cpu_to_le32(input); \
		(output) += 4; \
	} while (0)

/* Android wakelock abstraction */
#ifdef CONFIG_SCSC_WLAN_ANDROID
#define slsi_wake_lock_init(lock, type, name)	wake_lock_init(lock, type, name)
#define slsi_wake_lock(lock)					wake_lock(lock)
#define slsi_wake_unlock(lock)					wake_unlock(lock)
#define slsi_wake_lock_timeout(lock, timeout)	wake_lock_timeout(lock, timeout)
#define slsi_wake_lock_active(lock)				wake_lock_active(lock)
#define slsi_wake_lock_destroy(lock)			wake_lock_destroy(lock)
#else
#define slsi_wake_lock_init(lock, type, name)
#define slsi_wake_lock(lock)
#define slsi_wake_unlock(lock)
#define slsi_wake_lock_timeout(lock, timeout)
#define slsi_wake_lock_active(lock)				(false)
#define slsi_wake_lock_destroy(lock)
#endif

struct slsi_spinlock {
	/* a std spinlock */
	spinlock_t    lock;
	unsigned long flags;
};

/* Spinlock create can't fail, so return success regardless. */
static inline void slsi_spinlock_create(struct slsi_spinlock *lock)
{
	spin_lock_init(&lock->lock);
}

static inline void slsi_spinlock_lock(struct slsi_spinlock *lock)
{
	spin_lock_bh(&lock->lock);
}

static inline void slsi_spinlock_unlock(struct slsi_spinlock *lock)
{
	spin_unlock_bh(&lock->lock);
}

struct slsi_dev;
struct slsi_skb_work {
	struct slsi_dev         *sdev;
	struct net_device       *dev;   /* This can be NULL */
	struct workqueue_struct *workqueue;
	struct work_struct      work;
	struct sk_buff_head     queue;
	void __rcu              *sync_ptr;
};

static inline int slsi_skb_work_init(struct slsi_dev *sdev, struct net_device *dev, struct slsi_skb_work *work,
				     const char *name, void (*func)(struct work_struct *work))
{
	rcu_assign_pointer(work->sync_ptr, (void *)sdev);
	work->sdev = sdev;
	work->dev = dev;
	skb_queue_head_init(&work->queue);
	INIT_WORK(&work->work, func);
	work->workqueue = alloc_ordered_workqueue(name, 0);

	if (!work->workqueue)
		return -ENOMEM;
	return 0;
}

static inline void slsi_skb_schedule_work(struct slsi_skb_work *work)
{
	queue_work(work->workqueue, &work->work);
}

static inline void slsi_skb_work_enqueue_l(struct slsi_skb_work *work, struct sk_buff *skb)
{
	void *sync_ptr;

	rcu_read_lock();

	sync_ptr = rcu_dereference(work->sync_ptr);

	if (WARN_ON(!sync_ptr)) {
		kfree_skb(skb);
		rcu_read_unlock();
		return;
	}
	skb_queue_tail(&work->queue, skb);
	slsi_skb_schedule_work(work);

	rcu_read_unlock();
}

static inline struct sk_buff *slsi_skb_work_dequeue_l(struct slsi_skb_work *work)
{
	return skb_dequeue(&work->queue);
}

static inline void slsi_skb_work_deinit(struct slsi_skb_work *work)
{
	rcu_read_lock();

	if (WARN_ON(!work->sync_ptr)) {
		rcu_read_unlock();
		return;
	}

	rcu_assign_pointer(work->sync_ptr, NULL);
	rcu_read_unlock();

	synchronize_rcu();
	flush_workqueue(work->workqueue);
	destroy_workqueue(work->workqueue);
	work->workqueue = NULL;
	skb_queue_purge(&work->queue);
}

static inline void slsi_cfg80211_put_bss(struct wiphy *wiphy, struct cfg80211_bss *bss)
{
	cfg80211_put_bss(wiphy, bss);
}

#define slsi_skb_work_enqueue(work_, skb_) slsi_skb_work_enqueue_l(work_, skb_)
#define slsi_skb_work_dequeue(work_) slsi_skb_work_dequeue_l(work_)

static inline void slsi_eth_zero_addr(u8 *addr)
{
	memset(addr, 0x00, ETH_ALEN);
}

static inline void slsi_eth_broadcast_addr(u8 *addr)
{
	memset(addr, 0xff, ETH_ALEN);
}

static inline int slsi_str_to_int(char *str, int *result)
{
	int i = 0;
	int sign = 1;
	int err = 0;
	long long int res = 0;
	int digit = 0;

	if (!str)
		return 0;
	if (*str == '-') {
		sign = -1;
		++str;
	} else if (*str == '+') {
		sign = 1;
		++str;
	}

	*result = 0;
	if ((str[i] >= '0') && (str[i] <= '9')) {
		while (str[i] >= '0' && str[i] <= '9') {
			if (res > INT_MAX / 10) {
				err = 1;
				break;
			}
			res *= 10;
			digit = str[i] - '0';

			if (res > INT_MAX - digit) {
				if (sign == -1) {
					res += digit;
					if (-(res) >= INT_MIN) {
						break;
					} else {
						err = 1;
						break;
					}
				} else {
					err = 1;
					break;
				}
			}
			res += digit;
			i++;
		}

		if (!err)
			*result = ((sign == -1) ? -(res) : res);
		else
			return 0;
	}
	return i;
}

static inline int slsi_str_cmp(const char *s1, const char *s2)
{
	if (!s1 && !s2)
		return 0;
	if (s1 && !s2)
		return -1;
	if (!s1 && s2)
		return 1;

	return strcmp(s1, s2);
}

#define P80211_OUI_LEN		3

struct ieee80211_snap_hdr {
	u8 dsap;                /* always 0xAA */
	u8 ssap;                /* always 0xAA */
	u8 ctrl;                /* always 0x03 */
	u8 oui[P80211_OUI_LEN]; /* organizational universal id */
} __packed;

struct msdu_hdr {
	unsigned char             da[ETH_ALEN];
	unsigned char             sa[ETH_ALEN];
	__be16                    length;
	struct ieee80211_snap_hdr snap;
	__be16                    ether_type;
} __packed;

#define ETHER_TYPE_SIZE		2
#define MSDU_HLEN		sizeof(struct msdu_hdr)
#define MSDU_LENGTH		(sizeof(struct ieee80211_snap_hdr) + sizeof(__be16))

static inline int slsi_skb_msdu_to_ethhdr(struct sk_buff *skb)
{
	struct ethhdr   *eth;
	struct msdu_hdr *msdu;

	unsigned char   da[ETH_ALEN];
	unsigned char   sa[ETH_ALEN];
	__be16          proto;

	msdu = (struct msdu_hdr *)skb->data;
	SLSI_ETHER_COPY(da, msdu->da);
	SLSI_ETHER_COPY(sa, msdu->sa);
	proto = msdu->ether_type;

	skb_pull(skb, MSDU_HLEN);

	eth = (struct ethhdr *)skb_push(skb, ETH_HLEN);

	SLSI_ETHER_COPY(eth->h_dest, da);
	SLSI_ETHER_COPY(eth->h_source, sa);
	eth->h_proto = proto;

	return 0;
}

static inline int slsi_skb_ethhdr_to_msdu(struct sk_buff *skb)
{
	struct ethhdr   *eth;
	struct msdu_hdr *msdu;
	unsigned int    len;
	__be16          ether_type;

	if (skb_headroom(skb) < (MSDU_HLEN - ETH_HLEN))
		return -EINVAL;

	eth = eth_hdr(skb);
	ether_type = eth->h_proto;

	len = skb->len;

	skb_pull(skb, ETH_HLEN);

	msdu = (struct msdu_hdr *)skb_push(skb, MSDU_HLEN);

	SLSI_ETHER_COPY(msdu->da, eth->h_dest);
	SLSI_ETHER_COPY(msdu->sa, eth->h_source);
	msdu->length = htons(len - ETH_HLEN + MSDU_LENGTH);
	memcpy(&msdu->snap, rfc1042_header, sizeof(struct ieee80211_snap_hdr));
	msdu->ether_type = ether_type;

	return 0;
}

static inline u32 slsi_get_center_freq1(struct slsi_dev *sdev, u16 chann_info, u16 center_freq)
{
	u32 center_freq1 = 0x0000;

	SLSI_UNUSED_PARAMETER(sdev);

	switch (chann_info & 0xFF) {
	case 40:
		center_freq1 = center_freq - 20 * ((chann_info & 0xFF00) >> 8) + 10;
		break;
	case 80:
		center_freq1 = center_freq - 20 * ((chann_info & 0xFF00) >> 8) + 30;
		break;
	default:
		break;
	}
	return center_freq1;
}

/* Name: strtoint
 * Desc: Converts a string to a decimal or hexadecimal integer
 * s: the string to be converted
 * res: pointer to the calculated integer
 * return: 0 (success), 1(failure)
 */
static inline int strtoint(const char *s, int *res)
{
	int base = 10;

	if (strlen(s) > 2)
		if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
			base = 16;
	return kstrtoint(s, base, res);
}

static inline u8 *slsi_mem_dup(u8 *src, size_t len)
{
	u8 *dest;

	if (!src || !len)
		return NULL;

	dest = kmalloc(len, GFP_KERNEL);
	if (!dest)
		return NULL;
	memcpy(dest, src, len);
	return dest;
}

static inline void slsi_get_random_bytes(u8 *byte_buffer, u32 buffer_len)
{
	return get_random_bytes(byte_buffer, buffer_len);
}

static inline int slsi_util_nla_get_u8(const struct nlattr *attr, u8 *val)
{
	if (nla_len(attr) >= sizeof(u8)) {
		*val = nla_get_u8(attr);
		return 0;
	}
	return -EINVAL;
}

static inline int slsi_util_nla_get_u16(const struct nlattr *attr, u16 *val)
{
	if (nla_len(attr) >= sizeof(u16)) {
		*val = nla_get_u16(attr);
		return 0;
	}
	return -EINVAL;

}

static inline int slsi_util_nla_get_u32(const struct nlattr *attr, u32 *val)
{
	if (nla_len(attr) >= sizeof(u32)) {
		*val = nla_get_u32(attr);
		return 0;
	}
	return -EINVAL;
}

static inline int slsi_util_nla_get_u64(const struct nlattr *attr, u64 *val)
{
	if (nla_len(attr) >= sizeof(u64)) {
		*val = nla_get_u64(attr);
		return 0;
	}
	return -EINVAL;
}


static inline int slsi_util_nla_get_s8(const struct nlattr *attr, s8 *val)
{
	if (nla_len(attr) >= sizeof(s8)) {
		*val = nla_get_s8(attr);
		return 0;
	}
	return -EINVAL;
}

static inline int slsi_util_nla_get_s16(const struct nlattr *attr, s16 *val)
{
	if (nla_len(attr) >= sizeof(s16)) {
		*val = nla_get_s16(attr);
		return 0;
	}
	return -EINVAL;
}

static inline int slsi_util_nla_get_s32(const struct nlattr *attr, s32 *val)
{
	if (nla_len(attr) >= sizeof(s32)) {
		*val = nla_get_s32(attr);
		return 0;
	}
	return -EINVAL;
}

static inline int slsi_util_nla_get_s64(const struct nlattr *attr, s64 *val)
{
	if (nla_len(attr) >= sizeof(s64)) {
		*val = nla_get_s64(attr);
		return 0;
	}
	return -EINVAL;
}

static inline int slsi_util_nla_get_be16(const struct nlattr *attr, __be16 *val)
{
	if (nla_len(attr) >= sizeof(__be16)) {
		*val = nla_get_be16(attr);
		return 0;
	}
	return -EINVAL;
}

static inline int slsi_util_nla_get_be32(const struct nlattr *attr, __be32 *val)
{
	if (nla_len(attr) >= sizeof(__be32)) {
		*val = nla_get_be32(attr);
		return 0;
	}
	return -EINVAL;
}

static inline int slsi_util_nla_get_be64(const struct nlattr *attr, __be64 *val)
{
	if (nla_len(attr) >= sizeof(__be64)) {
		*val = nla_get_be64(attr);
		return 0;
	}
	return -EINVAL;
}

static inline int slsi_util_nla_get_le16(const struct nlattr *attr,  __le16  *val)
{
	if (nla_len(attr) >= sizeof(__le16)) {
		*val = nla_get_le16(attr);
		return 0;
	}
	return -EINVAL;
}

static inline int slsi_util_nla_get_le32(const struct nlattr *attr, __le32 *val)
{
	if (nla_len(attr) >= sizeof(__le32)) {
		*val = nla_get_le32(attr);
		return 0;
	}
	return -EINVAL;
}

static inline int slsi_util_nla_get_le64(const struct nlattr *attr, __le64 *val)
{
	if (nla_len(attr) >= sizeof(__le64)) {
		*val = nla_get_le64(attr);
		return 0;
	}
	return -EINVAL;
}

static inline int slsi_util_nla_get_data(const struct nlattr *attr, size_t size, void *val)
{
	if (nla_len(attr) >= size) {
		memcpy(val, nla_data(attr), size);
		return 0;
	}
	return -EINVAL;
}

static inline struct net_device *slsi_ndev_vif_2_net_device(struct netdev_vif *ndev_vif)
{
	unsigned long int address = (unsigned long int)ndev_vif;
	unsigned int offset = ALIGN(sizeof(struct net_device), NETDEV_ALIGN);

	if (address < offset)
		return NULL;
	return (struct net_device *)((char*)ndev_vif - offset);
}

static inline void get_kernel_timestamp(int *time)
{
	struct timespec64 tv;

	ktime_get_ts64(&tv);
	time[0] = tv.tv_sec;
	time[1] = tv.tv_nsec;
}

#ifdef __cplusplus
}
#endif

#endif /* SLSI_UTILS_H__ */
