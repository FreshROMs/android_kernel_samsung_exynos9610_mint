/*****************************************************************************
 *
 * Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/version.h>
#include <linux/hardirq.h>
#include <linux/cpufreq.h>

#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <asm/page.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <scsc/scsc_mx.h>
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
#include <scsc/scsc_log_collector.h>
#endif
#include <linux/delay.h>
#include <linux/mutex.h>
#include "hip4_sampler.h"

#include "debug.h"

struct hip4_record {
	u32     record_num;
	ktime_t ts;
	u32     record;
	u32     record2;
} __packed;

static atomic_t in_read;

/* Create a global spinlock for all the instances */
/* It is less efficent, but we simplify the implementation */
static DEFINE_SPINLOCK(g_spinlock);

static bool hip4_sampler_enable = true;
module_param(hip4_sampler_enable, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_sampler_enable, "Enable hip4_sampler_enable. Run-time option - (default: Y)");

static bool hip4_sampler_dynamic = true;
module_param(hip4_sampler_dynamic, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_sampler_dynamic, "Enable hip4_sampler dynamic adaptation based on TPUT. Run-time option - (default: Y)");

static int hip4_sampler_kfifo_len = 128 * 1024;
module_param(hip4_sampler_kfifo_len, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_sampler_kfifo_len, "Streaming fifo buffer length in num of records- default: 262144 Max: 262144. Loaded at /dev open");

static int hip4_sampler_static_kfifo_len = 128 * 1024;
module_param(hip4_sampler_static_kfifo_len, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_sampler_static_kfifo_len, "Offline fifo buffer length in num of records- default: 262144 Max: 262144. Loaded at /dev open");

bool hip4_sampler_sample_q = true;
module_param(hip4_sampler_sample_q, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_sampler_sample_q, "Sample Queues. Default: Y. Run time option");

bool hip4_sampler_sample_qref = true;
module_param(hip4_sampler_sample_qref, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_sampler_sample_qref, "Sample Queue References. Default: Y. Run time option");

bool hip4_sampler_sample_int = true;
module_param(hip4_sampler_sample_int, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_sampler_sample_int, "Sample WQ/Tasklet Intr BH in/out. Default: Y. Run time option");

bool hip4_sampler_sample_fapi = true;
module_param(hip4_sampler_sample_fapi, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_sampler_sample_fapi, "Sample FAPI ctrl signals. Default: Y. Run time option");

bool hip4_sampler_sample_through = true;
module_param(hip4_sampler_sample_through, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_sampler_sample_through, "Sample throughput. Default: Y. Run time option");

bool hip4_sampler_sample_tcp = true;
module_param(hip4_sampler_sample_tcp, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_sampler_sample_tcp, "Sample TCP streams. Default: Y. Run time option");

bool hip4_sampler_sample_start_stop_q = true;
module_param(hip4_sampler_sample_start_stop_q, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_sampler_sample_start_stop_q, "Sample Stop/Start queues. Default: Y. Run time option");

bool hip4_sampler_sample_mbulk = true;
module_param(hip4_sampler_sample_mbulk, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_sampler_sample_mbulk, "Sample Mbulk counter. Default: Y. Run time option");

bool hip4_sampler_sample_qfull;
module_param(hip4_sampler_sample_qfull, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_sampler_sample_qfull, "Sample Q full event. Default: N. Run time option");

bool hip4_sampler_sample_mfull = true;
module_param(hip4_sampler_sample_mfull, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_sampler_sample_mfull, "Sample Mbulk full event. Default: Y. Run time option");

bool hip4_sampler_vif = true;
module_param(hip4_sampler_vif, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_sampler_vif, "Sample VIF. Default: Y. Run time option");

bool hip4_sampler_bot = true;
module_param(hip4_sampler_bot, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_sampler_bot, "Sample BOT. Default: Y. Run time option");

bool hip4_sampler_pkt_tx = true;
module_param(hip4_sampler_pkt_tx, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_sampler_pkt_tx, "Track TX Data packet TX->HIP4->FB. Default: Y. Run time option");

bool hip4_sampler_suspend_resume = true;
module_param(hip4_sampler_suspend_resume, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_sampler_suspend_resume, "Sample Suspend/Resume events. Default: Y. Run time option");

#define HIP4_TPUT_HLIMIT	400000000  /* 400Mbps */
#define HIP4_TPUT_LLIMIT	350000000  /* 350Mbps */
#define HIP4_TPUT_HSECONDS	1
#define HIP4_TPUT_LSECONDS	5

static bool hip4_sampler_sample_q_hput;
static bool hip4_sampler_sample_qref_hput;
static bool hip4_sampler_sample_int_hput;
static bool hip4_sampler_sample_fapi_hput;
/* static bool hip4_sampler_sample_through_hput; */
/* static bool hip4_sampler_sample_start_stop_q_hput; */
/* static bool hip4_sampler_sample_mbulk_hput; */
/* static bool hip4_sampler_sample_qfull_hput; */
/* static bool hip4_sampler_sample_mfull_hput; */
static bool hip4_sampler_vif_hput;
static bool hip4_sampler_bot_hput;
static bool hip4_sampler_pkt_tx_hput;
static bool hip4_sampler_suspend_resume_hput;

static bool hip4_sampler_in_htput;
static u16 hip4_sampler_in_htput_seconds;

#define DRV_NAME                "hip4_sampler"
#define DEVICE_NAME             "hip4_sampler"

#ifndef VM_RESERVED
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define VER_MAJOR               0
#define VER_MINOR               0

DECLARE_BITMAP(bitmap_hip4_sampler_minor, SCSC_HIP4_DEBUG_INTERFACES);

enum hip4_dg_errors {
	NO_ERROR = 0,
	BUFFER_OVERFLOW,
	KFIFO_ERROR,
	KFIFO_FULL,
};

enum hip4_type {
	STREAMING = 0,
	OFFLINE,
};

struct hip4_sampler_dev {
	/* file pointer */
	struct file       *filp;
	/* char device */
	struct cdev       cdev;
	/*device pointer*/
	struct device     *dev;
	/*mx pointer*/
	struct scsc_mx    *mx;
	/* Associated kfifo */
	DECLARE_KFIFO_PTR(fifo, struct hip4_record);
	/* Associated read_wait queue.*/
	wait_queue_head_t read_wait;
	/* Device in error */
	enum hip4_dg_errors    error;
	/* Device node mutex for fops */
	struct mutex      mutex;
	/* Record number */
	u32               record_num;
	/* To profile kfifo num elements */
	u32               kfifo_max;
	/* Sampler type streaming/offline */
	enum hip4_type         type;
	/* reference to minor number */
	u32               minor;
};

/**
 * SCSC User Space debug sampler interface (singleton)
 */
static struct {
	bool                    init;
	dev_t                   device;
	struct class            *class_hip4_sampler;
	struct hip4_sampler_dev devs[SCSC_HIP4_DEBUG_INTERFACES];
} hip4_sampler;

void __hip4_sampler_update_record(struct hip4_sampler_dev *hip4_dev, u32 minor, u8 param1, u8 param2, u8 param3, u8 param4, u32 param5)
{
	struct hip4_record      ev;
	u32                     ret;
	u32                     cpu;
	u32                     freq = 0;

	/* If char device if open, use streaming buffer */
	if (hip4_dev->filp) {
		/* put string into the Streaming fifo */
		if (kfifo_avail(&hip4_dev->fifo)) {
			/* Push values in Fifo*/
			cpu = smp_processor_id();
			if (hip4_dev->record_num % 64 == 0)
				freq = (cpufreq_quick_get(cpu) / 1000) & 0xfff;
			ev.record_num = (0x0000ffff & hip4_dev->record_num++) | (cpu << 28) | (freq << 16);
			ev.ts = ktime_get();
			ev.record = ((param1 & 0xff) << 24) | ((param2 & 0xff) << 16) | ((param3 & 0xff) << 8) | (param4 & 0xff);
			ev.record2 = param5;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0))
			kfifo_put(&hip4_dev->fifo, ev);
#else
			kfifo_put(&hip4_dev->fifo, &ev);
#endif
			ret = kfifo_len(&hip4_dev->fifo);
			if (ret > hip4_dev->kfifo_max)
				hip4_dev->kfifo_max = ret;
		} else {
			hip4_dev->error = KFIFO_FULL;
			return;
		}
		wake_up_interruptible(&hip4_dev->read_wait);
		/* If streaming buffer is not in use, put samples in offline buffer */
	} else {
		/* Get associated Offline buffer */
		hip4_dev = &hip4_sampler.devs[minor + 1];
		/* Record in offline fifo */
		/* if fifo is full, remove last item */
		if (kfifo_is_full(&hip4_dev->fifo))
			ret = kfifo_get(&hip4_dev->fifo, &ev);
		cpu = smp_processor_id();
		if (hip4_dev->record_num % 64 == 0)
			freq = (cpufreq_quick_get(cpu) / 1000) & 0xfff;
		/* Push values in Static Fifo*/
		ev.record_num = (0x0000ffff & hip4_dev->record_num++) | (cpu << 28) | (freq << 16);
		ev.ts = ktime_get();
		ev.record = ((param1 & 0xff) << 24) | ((param2 & 0xff) << 16) | ((param3 & 0xff) << 8) | (param4 & 0xff);
		ev.record2 = param5;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0))
		kfifo_put(&hip4_dev->fifo, ev);
#else
		kfifo_put(&hip4_dev->fifo, &ev);
#endif
	}
}

void hip4_sampler_update_record(u32 minor, u8 param1, u8 param2, u8 param3, u8 param4, u32 param5)
{
	struct hip4_sampler_dev *hip4_dev;
	unsigned long flags;

	if (!hip4_sampler_enable || !hip4_sampler.init)
		return;

	if (atomic_read(&in_read))
		return;

	if (minor >= SCSC_HIP4_INTERFACES)
		return;

	spin_lock_irqsave(&g_spinlock, flags);
	hip4_dev = &hip4_sampler.devs[minor];
	__hip4_sampler_update_record(hip4_dev, minor, param1, param2, param3, param4, param5);
	spin_unlock_irqrestore(&g_spinlock, flags);
}

static void hip4_sampler_store_param(void)
{
	hip4_sampler_sample_q_hput = hip4_sampler_sample_q;
	hip4_sampler_sample_qref_hput = hip4_sampler_sample_qref;
	hip4_sampler_sample_int_hput = hip4_sampler_sample_int;
	hip4_sampler_sample_fapi_hput = hip4_sampler_sample_fapi;
	/* hip4_sampler_sample_through_hput = hip4_sampler_sample_through; */
	/* hip4_sampler_sample_start_stop_q_hput = hip4_sampler_sample_start_stop_q; */
	/* hip4_sampler_sample_mbulk_hput = hip4_sampler_sample_mbulk; */
	/* hip4_sampler_sample_qfull_hput = hip4_sampler_sample_qfull; */
	/* hip4_sampler_sample_mfull_hput = hip4_sampler_sample_mfull; */
	hip4_sampler_vif_hput = hip4_sampler_vif;
	hip4_sampler_bot_hput = hip4_sampler_bot;
	hip4_sampler_pkt_tx_hput = hip4_sampler_pkt_tx;
	hip4_sampler_suspend_resume_hput = hip4_sampler_suspend_resume;

	/* Reset values to avoid contention */
	hip4_sampler_sample_q = false;
	hip4_sampler_sample_qref = false;
	hip4_sampler_sample_int = false;
	hip4_sampler_sample_fapi = false;
	/* hip4_sampler_sample_through_hput = false; */
	/* hip4_sampler_sample_start_stop_q_hput = false; */
	/* hip4_sampler_sample_mbulk = false; */
	/* hip4_sampler_sample_qfull_hput = false; */
	/* hip4_sampler_sample_mfull_hput = false; */
	hip4_sampler_vif = false;
	hip4_sampler_bot = false;
	hip4_sampler_pkt_tx = false;
	hip4_sampler_suspend_resume = false;
}

static void hip4_sampler_restore_param(void)
{
	hip4_sampler_sample_q = hip4_sampler_sample_q_hput;
	hip4_sampler_sample_qref = hip4_sampler_sample_qref_hput;
	hip4_sampler_sample_int = hip4_sampler_sample_int_hput;
	hip4_sampler_sample_fapi = hip4_sampler_sample_fapi_hput;
	/* hip4_sampler_sample_through = hip4_sampler_sample_through_hput; */
	/* hip4_sampler_sample_start_stop_q = hip4_sampler_sample_start_stop_q_hput; */
	/* hip4_sampler_sample_mbulk = hip4_sampler_sample_mbulk_hput; */
	/* hip4_sampler_sample_qfull = hip4_sampler_sample_qfull_hput; */
	/* hip4_sampler_sample_mfull = hip4_sampler_sample_mfull_hput; */
	hip4_sampler_vif = hip4_sampler_vif_hput;
	hip4_sampler_bot = hip4_sampler_bot_hput;
	hip4_sampler_pkt_tx = hip4_sampler_pkt_tx_hput;
	hip4_sampler_suspend_resume = hip4_sampler_suspend_resume_hput;
}

static void hip4_sampler_dynamic_switcher(u32 bps)
{
	/* Running in htput, */
	if (hip4_sampler_in_htput) {
		if (bps < HIP4_TPUT_LLIMIT) {
			/* bps went down , count number of times to switch */
			hip4_sampler_in_htput_seconds++;
			if (hip4_sampler_in_htput_seconds >= HIP4_TPUT_LSECONDS) {
			/* HIP4_TPUT_LSECONDS have passed, switch to low tput samples */
				hip4_sampler_in_htput = false;
				hip4_sampler_in_htput_seconds = 0;
				hip4_sampler_restore_param();
			}
		} else {
			hip4_sampler_in_htput_seconds = 0;
		}
	} else {
		if (bps > HIP4_TPUT_HLIMIT) {
			/* bps went up, count number of times to switch */
			hip4_sampler_in_htput_seconds++;
			if (hip4_sampler_in_htput_seconds >= HIP4_TPUT_HSECONDS) {
			/* HIP4_TPUT_LSECONDS have passed, switch to high tput samples */
				hip4_sampler_in_htput = true;
				hip4_sampler_in_htput_seconds = 0;
				hip4_sampler_store_param();
			}
		} else {
			hip4_sampler_in_htput_seconds = 0;
		}
	}
}

static u32 g_tput_rx;
static u32 g_tput_tx;

void hip4_sampler_tput_monitor(void *client_ctx, u32 state, u32 tput_tx, u32 tput_rx)
{
	struct hip4_sampler_dev *hip4_sampler_dev = (struct hip4_sampler_dev *)client_ctx;

	if (!hip4_sampler_enable)
		return;

	if ((g_tput_tx == tput_tx) && (g_tput_rx == tput_rx))
		return;

	g_tput_tx = tput_tx;
	g_tput_rx = tput_rx;

	if (hip4_sampler_dynamic) {
		/* Call the dynamic switcher with the computed bps
		 * The algorithm will decide to not change, decrease
		 * or increase the sampler verbosity
		 */
		if (tput_rx > tput_tx)
			hip4_sampler_dynamic_switcher(tput_rx);
		else
			hip4_sampler_dynamic_switcher(tput_tx);
	}

	/* Generate the TX sample, in bps, Kbps, or Mbps */
	if (tput_tx < 1000) {
		SCSC_HIP4_SAMPLER_THROUG(hip4_sampler_dev->minor, 1, (tput_tx & 0xff00) >> 8, tput_tx & 0xff);
	} else if ((tput_tx >= 1000) && (tput_tx < (1000 * 1000))) {
		tput_tx = tput_tx / 1000;
		SCSC_HIP4_SAMPLER_THROUG_K(hip4_sampler_dev->minor, 1, (tput_tx & 0xff00) >> 8, tput_tx & 0xff);
	} else {
		tput_tx = tput_tx / (1000 * 1000);
		SCSC_HIP4_SAMPLER_THROUG_M(hip4_sampler_dev->minor, 1, (tput_tx & 0xff00) >> 8, tput_tx & 0xff);
	}

	/* Generate the RX sample, in bps, Kbps, or Mbps */
	if (tput_rx < 1000) {
		SCSC_HIP4_SAMPLER_THROUG(hip4_sampler_dev->minor, 0, (tput_rx & 0xff00) >> 8, tput_rx & 0xff);
	} else if ((tput_rx >= 1000) && (tput_rx < (1000 * 1000))) {
		tput_rx = tput_rx / 1000;
		SCSC_HIP4_SAMPLER_THROUG_K(hip4_sampler_dev->minor, 0, (tput_rx & 0xff00) >> 8, tput_rx & 0xff);
	} else {
		tput_rx = tput_rx / (1000 * 1000);
		SCSC_HIP4_SAMPLER_THROUG_M(hip4_sampler_dev->minor, 0, (tput_rx & 0xff00) >> 8, tput_rx & 0xff);
	}
}

void hip4_sampler_tcp_decode(struct slsi_dev *sdev, struct net_device *dev, u8 *frame, bool from_ba)
{
	struct tcphdr *tcp_hdr;
	struct ethhdr *ehdr = (struct ethhdr *)(frame);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u8 *ip_frame;
	u16 ip_data_offset;
	u8 hlen;
	u16 len;
	u8 proto;
	u8 idx;

	if (be16_to_cpu(ehdr->h_proto) != ETH_P_IP)
		return;

	ip_frame = frame + ETH_HLEN;
	proto = ip_frame[9];

	if (proto != IPPROTO_TCP)
		return;

	ip_data_offset = 20;
	hlen = ip_frame[0] & 0x0F;
	len = ip_frame[2] << 8 | ip_frame[3];

	if (hlen > 5)
		ip_data_offset += (hlen - 5) * 4;

	tcp_hdr = (struct tcphdr *)(ip_frame + ip_data_offset);

	slsi_spinlock_lock(&ndev_vif->tcp_ack_lock);
	/* Search for an existing record on this connection. */
	for (idx = 0; idx < TCP_ACK_SUPPRESSION_RECORDS_MAX; idx++) {
		struct slsi_tcp_ack_s *tcp_ack;
		u32 rwnd = 0;

		tcp_ack = &ndev_vif->ack_suppression[idx];
		if ((tcp_ack->dport == tcp_hdr->source) && (tcp_ack->sport == tcp_hdr->dest)) {
			if (from_ba && tcp_hdr->syn && tcp_hdr->ack) {
				unsigned char *options;
				u32 optlen = 0, len = 0;

				if (tcp_hdr->doff > 5)
					optlen = (tcp_hdr->doff - 5) * 4;

				options = (u8 *)tcp_hdr + TCP_ACK_SUPPRESSION_OPTIONS_OFFSET;

				while (optlen > 0) {
					switch (options[0]) {
					case TCP_ACK_SUPPRESSION_OPTION_EOL:
						len = 1;
						break;
					case TCP_ACK_SUPPRESSION_OPTION_NOP:
						len = 1;
						break;
					case TCP_ACK_SUPPRESSION_OPTION_WINDOW:
						tcp_ack->rx_window_scale = options[2];
						len = options[1];
						break;
					default:
						len = options[1];
						break;
					}
					/* if length field in TCP options is 0, or greater than
					 * total options length, then options are incorrect
					 */
					if ((len == 0) || (len >= optlen))
						break;

					if (optlen >= len)
						optlen -= len;
					else
						optlen = 0;
					options += len;
				}
			}
			if (len > ((hlen * 4) + (tcp_hdr->doff * 4))) {
				if (from_ba)
					SCSC_HIP4_SAMPLER_TCP_DATA(sdev->minor_prof, tcp_ack->stream_id, tcp_hdr->seq);
				else
					SCSC_HIP4_SAMPLER_TCP_DATA_IN(sdev->minor_prof, tcp_ack->stream_id, tcp_hdr->seq);
			} else {
				if (tcp_ack->rx_window_scale)
					rwnd = be16_to_cpu(tcp_hdr->window) * (2 << tcp_ack->rx_window_scale);
				else
					rwnd = be16_to_cpu(tcp_hdr->window);
				if (from_ba) {
					SCSC_HIP4_SAMPLER_TCP_ACK(sdev->minor_prof, tcp_ack->stream_id, be32_to_cpu(tcp_hdr->ack_seq));
					SCSC_HIP4_SAMPLER_TCP_RWND(sdev->minor_prof, tcp_ack->stream_id, rwnd);
				} else {
					SCSC_HIP4_SAMPLER_TCP_ACK_IN(sdev->minor_prof, tcp_ack->stream_id, be32_to_cpu(tcp_hdr->ack_seq));
				}
			}
			break;
		}
	}
	slsi_spinlock_unlock(&ndev_vif->tcp_ack_lock);
}

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
int hip4_collect_init(struct scsc_log_collector_client *collect_client)
{
	/* Stop Sampling */
	atomic_set(&in_read, 1);
	return 0;
}

int hip4_collect(struct scsc_log_collector_client *collect_client, size_t size)
{
	int i = SCSC_HIP4_DEBUG_INTERFACES;
	int ret = 0;
	unsigned long flags;
	u32 num_samples;
	struct hip4_sampler_dev *hip4_dev;
	void *buf;
	struct scsc_hip4_sampler_header header;


	SLSI_INFO_NODEV("Triggered log collection in hip4_sampler\n");

	if (!hip4_sampler_enable)
		return 0;

	while (i--)
		if (hip4_sampler.devs[i].mx == collect_client->prv && hip4_sampler.devs[i].type == OFFLINE) {
			hip4_dev = &hip4_sampler.devs[i];
			num_samples = kfifo_len(&hip4_dev->fifo);
			if (!num_samples)
				continue;
			buf = vmalloc(num_samples * sizeof(struct hip4_record));
			if (!buf)
				continue;
			/* Write the hip4 sampler header */
			memset(&header, 0, sizeof(header));
			memcpy(header.magic, SCSC_HIP4_SAMPLER_MAGIC, 4);
			header.offset_data = sizeof(header);
			header.version_major = SCSC_HIP4_SAMPLER_HEADER_VERSION_MAJOR;
			header.version_minor = SCSC_HIP4_SAMPLER_HEADER_VERSION_MINOR;
#ifdef CONFIG_SOC_EXYNOS9630
			header.platform = SCSC_HIP4_SAMPLER_EXYNOS9630;
#elif defined(CONFIG_SOC_EXYNOS9610)
			header.platform = SCSC_HIP4_SAMPLER_EXYNOS9610;
#elif defined(CONFIG_SOC_EXYNOS7885)
			header.platform = SCSC_HIP4_SAMPLER_EXYNOS7885;
#else
			header.platform = SCSC_HIP4_SAMPLER_UNDEF;
#endif
			/* We are currently using only one sample type */
			header.sample_type = SCSC_HIP4_SAMPLER_TYPE_TCP;
			if (hip4_sampler_in_htput)
				header.hip4_status |= BIT(0);
#ifdef CONFIG_DEBUG_SPINLOCK
			header.hip4_status |= BIT(1);
#endif
			header.num_samples = num_samples;

			ret = scsc_log_collector_write((char *)&header, sizeof(header), 1);
			if (ret)
				goto error;

			spin_lock_irqsave(&g_spinlock, flags);
			ret = kfifo_out(&hip4_dev->fifo, buf, num_samples);
			spin_unlock_irqrestore(&g_spinlock, flags);
			if (!ret)
				goto error;
			SLSI_DBG1_NODEV(SLSI_HIP, "num_samples %d ret %d size of hip4_record %zu\n", num_samples, ret, sizeof(struct hip4_record));
			ret = scsc_log_collector_write(buf, ret * sizeof(struct hip4_record), 1);
			if (ret)
				goto error;
			vfree(buf);
		}
	return 0;
error:
	vfree(buf);
	return ret;
}

int hip4_collect_end(struct scsc_log_collector_client *collect_client)
{
	/* Restart sampling */
	atomic_set(&in_read, 0);
	return 0;
}

/* Collect client registration */
struct scsc_log_collector_client hip4_collect_client = {
	.name = "HIP4 Sampler",
	.type = SCSC_LOG_CHUNK_HIP4_SAMPLER,
	.collect_init = hip4_collect_init,
	.collect = hip4_collect,
	.collect_end = hip4_collect_end,
	.prv = NULL,
};
#endif

static int hip4_sampler_open(struct inode *inode, struct file *filp)
{
	struct hip4_sampler_dev *hip4_dev;
	int                     ret = 0;

	hip4_dev = container_of(inode->i_cdev, struct hip4_sampler_dev, cdev);

	if (hip4_dev->type == OFFLINE) {
		/* Offline buffer skip open */
		filp->private_data = hip4_dev;
		return 0;
	}
	if (mutex_lock_interruptible(&hip4_dev->mutex))
		return -ERESTARTSYS;

	if (filp->private_data) {
		SLSI_INFO_NODEV("Service already started\n");
		ret = 0;
		goto end;
	}

	if (hip4_sampler_kfifo_len > 256 * 1024) {
		SLSI_DBG1_NODEV(SLSI_HIP, "hip4_sampler_kfifo_len %d > 2262144. Set to MAX", hip4_sampler_kfifo_len);
		hip4_sampler_kfifo_len = 256 * 1024;
	}

	ret = kfifo_alloc(&hip4_dev->fifo, hip4_sampler_kfifo_len, GFP_KERNEL);
	if (ret) {
		SLSI_ERR_NODEV("kfifo_alloc failed");
		ret = -ENOMEM;
		goto end;
	}

	filp->private_data = hip4_dev;

	/* Clear any remaining error */
	hip4_dev->error = NO_ERROR;

	hip4_dev->record_num = 0;
	hip4_dev->kfifo_max = 0;
	hip4_dev->filp = filp;

	SLSI_INFO_NODEV("%s: Sampling....\n", DRV_NAME);
end:
	mutex_unlock(&hip4_dev->mutex);
	return ret;
}

static ssize_t hip4_sampler_read(struct file *filp, char __user *buf, size_t len, loff_t *offset)
{
	unsigned int            copied;
	int                     ret = 0;
	struct hip4_sampler_dev *hip4_dev;

	hip4_dev = filp->private_data;

	if (hip4_dev->type == OFFLINE) {
		/* Offline buffer skip open */
		if (mutex_lock_interruptible(&hip4_dev->mutex))
			return -EINTR;
		atomic_set(&in_read, 1);
		ret = kfifo_to_user(&hip4_dev->fifo, buf, len, &copied);
		mutex_unlock(&hip4_dev->mutex);
		return ret ? ret : copied;
	}

	if (mutex_lock_interruptible(&hip4_dev->mutex))
		return -EINTR;

	/* Check whether the device is in error */
	if (hip4_dev->error != NO_ERROR) {
		SLSI_ERR_NODEV("Device in error\n");
		ret = -EIO;
		goto end;
	}

	while (len) {
		if (kfifo_len(&hip4_dev->fifo)) {
			ret = kfifo_to_user(&hip4_dev->fifo, buf, len, &copied);
			if (!ret)
				ret = copied;
			break;
		}

		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}

		ret = wait_event_interruptible(hip4_dev->read_wait,
					       !kfifo_is_empty(&hip4_dev->fifo));
		if (ret < 0)
			break;
	}
end:
	mutex_unlock(&hip4_dev->mutex);
	return ret;
}

static unsigned hip4_sampler_poll(struct file *filp, poll_table *wait)
{
	struct hip4_sampler_dev *hip4_dev;
	int                     ret;

	hip4_dev = filp->private_data;

	if (hip4_dev->type == OFFLINE)
		/* Offline buffer skip poll */
		return 0;

	if (mutex_lock_interruptible(&hip4_dev->mutex))
		return -EINTR;

	if (hip4_dev->error != NO_ERROR) {
		ret = POLLERR;
		goto end;
	}

	poll_wait(filp, &hip4_dev->read_wait, wait);

	if (!kfifo_is_empty(&hip4_dev->fifo)) {
		ret = POLLIN | POLLRDNORM;  /* readeable */
		goto end;
	}

	ret = POLLOUT | POLLWRNORM;         /* writable */

end:
	mutex_unlock(&hip4_dev->mutex);
	return ret;
}

static int hip4_sampler_release(struct inode *inode, struct file *filp)
{
	struct hip4_sampler_dev *hip4_dev;

	hip4_dev = container_of(inode->i_cdev, struct hip4_sampler_dev, cdev);

	if (hip4_dev->type == OFFLINE) {
		atomic_set(&in_read, 0);
		/* Offline buffer skip release */
		return 0;
	}

	if (mutex_lock_interruptible(&hip4_dev->mutex))
		return -EINTR;

	if (!hip4_dev->filp) {
		SLSI_ERR_NODEV("Device already closed\n");
		mutex_unlock(&hip4_dev->mutex);
		return -EIO;
	}

	if (hip4_dev != filp->private_data) {
		SLSI_ERR_NODEV("Data mismatch\n");
		mutex_unlock(&hip4_dev->mutex);
		return -EIO;
	}

	filp->private_data = NULL;
	hip4_dev->filp = NULL;
	kfifo_free(&hip4_dev->fifo);

	mutex_unlock(&hip4_dev->mutex);
	SLSI_INFO_NODEV("%s: Sampling... end. Kfifo_max = %d\n", DRV_NAME, hip4_dev->kfifo_max);
	return 0;
}

static const struct file_operations hip4_sampler_fops = {
	.owner          = THIS_MODULE,
	.open           = hip4_sampler_open,
	.read           = hip4_sampler_read,
	.release        = hip4_sampler_release,
	.poll           = hip4_sampler_poll,
};

/* Return minor (if exists) associated with this maxwell instance */
int hip4_sampler_register_hip(struct scsc_mx *mx)
{
	int i = SCSC_HIP4_DEBUG_INTERFACES;

	while (i--)
		if (hip4_sampler.devs[i].mx == mx &&
		    hip4_sampler.devs[i].type == STREAMING)
			return i;
	return -ENODEV;
}

void hip4_sampler_create(struct slsi_dev *sdev, struct scsc_mx *mx)
{
	dev_t devn;
	int   ret;
	char  dev_name[20];
	int   minor;
	int   i;

	SLSI_INFO_NODEV("hip4_sampler version: %d.%d\n", VER_MAJOR, VER_MINOR);

	memset(&hip4_sampler, 0, sizeof(hip4_sampler));
	/* Check whether exists */
	if (!hip4_sampler.init) {
		ret = alloc_chrdev_region(&hip4_sampler.device, 0, SCSC_HIP4_DEBUG_INTERFACES, "hip4_sampler_char");
		if (ret)
			goto error;

		hip4_sampler.class_hip4_sampler = class_create(THIS_MODULE, DEVICE_NAME);
		if (IS_ERR(hip4_sampler.class_hip4_sampler)) {
			SLSI_ERR_NODEV("hip4_sampler class creation failed\n");
			ret = PTR_ERR(hip4_sampler.class_hip4_sampler);
			goto error_class;
		}
	}

	/* Search for free minors */
	minor = find_first_zero_bit(bitmap_hip4_sampler_minor, SCSC_HIP4_DEBUG_INTERFACES);
	if (minor == SCSC_HIP4_DEBUG_INTERFACES) {
		SLSI_INFO_NODEV("minor %d > SCSC_TTY_MINORS\n", minor);
		return;
	}

	/* Create Stream channels */
	/* Each Stream channel will have an associated Offline channel */
	for (i = 0; i < SCSC_HIP4_STREAM_CH; i++) {
		minor += i;
		devn = MKDEV(MAJOR(hip4_sampler.device), MINOR(minor));

		snprintf(dev_name, sizeof(dev_name), "%s_%d_%s", "hip4", i, "sam_str");

		cdev_init(&hip4_sampler.devs[minor].cdev, &hip4_sampler_fops);
		hip4_sampler.devs[minor].cdev.owner = THIS_MODULE;
		hip4_sampler.devs[minor].cdev.ops = &hip4_sampler_fops;

		ret = cdev_add(&hip4_sampler.devs[minor].cdev, devn, 1);
		if (ret) {
			hip4_sampler.devs[minor].cdev.dev = 0;
			return;
		}

		hip4_sampler.devs[minor].dev =
			device_create(hip4_sampler.class_hip4_sampler, NULL, hip4_sampler.devs[minor].cdev.dev, NULL, dev_name);

		if (!hip4_sampler.devs[minor].dev) {
			SLSI_ERR_NODEV("dev is NULL\n");
			hip4_sampler.devs[minor].cdev.dev = 0;
			cdev_del(&hip4_sampler.devs[minor].cdev);
			return;
		}

		hip4_sampler.devs[minor].mx = mx;

		mutex_init(&hip4_sampler.devs[minor].mutex);
		hip4_sampler.devs[minor].kfifo_max = 0;
		hip4_sampler.devs[minor].type = STREAMING;
		hip4_sampler.devs[minor].minor = minor;

		init_waitqueue_head(&hip4_sampler.devs[minor].read_wait);

		slsi_traffic_mon_client_register(
			sdev,
			&hip4_sampler.devs[minor],
			TRAFFIC_MON_CLIENT_MODE_PERIODIC,
			0,
			0,
			hip4_sampler_tput_monitor);

		/* Update bit mask */
		set_bit(minor, bitmap_hip4_sampler_minor);

		minor++;

		/* Create associated offline channel */
		devn = MKDEV(MAJOR(hip4_sampler.device), MINOR(minor));

		snprintf(dev_name, sizeof(dev_name), "%s_%d_%s", "hip4", i, "sam_off");

		cdev_init(&hip4_sampler.devs[minor].cdev, &hip4_sampler_fops);
		hip4_sampler.devs[minor].cdev.owner = THIS_MODULE;
		hip4_sampler.devs[minor].cdev.ops = &hip4_sampler_fops;

		ret = cdev_add(&hip4_sampler.devs[minor].cdev, devn, 1);
		if (ret) {
			hip4_sampler.devs[minor].cdev.dev = 0;
			return;
		}

		hip4_sampler.devs[minor].dev =
			device_create(hip4_sampler.class_hip4_sampler, NULL, hip4_sampler.devs[minor].cdev.dev, NULL, dev_name);

		if (!hip4_sampler.devs[minor].dev) {
			hip4_sampler.devs[minor].cdev.dev = 0;
			cdev_del(&hip4_sampler.devs[minor].cdev);
			return;
		}

		if (hip4_sampler_static_kfifo_len > 256 * 1024) {
			SLSI_DBG1_NODEV(SLSI_HIP, "hip4_sampler_static_kfifo_len %d > 2262144. Set to MAX", hip4_sampler_static_kfifo_len);
			hip4_sampler_static_kfifo_len = 256 * 1024;
		}
		ret = kfifo_alloc(&hip4_sampler.devs[minor].fifo, hip4_sampler_static_kfifo_len, GFP_KERNEL);
		if (ret) {
			SLSI_ERR_NODEV("kfifo_alloc failed");
			hip4_sampler.devs[minor].dev = NULL;
			hip4_sampler.devs[minor].cdev.dev = 0;
			cdev_del(&hip4_sampler.devs[minor].cdev);
			return;
		}

		hip4_sampler.devs[minor].mx = mx;

		mutex_init(&hip4_sampler.devs[minor].mutex);
		hip4_sampler.devs[minor].kfifo_max = 0;
		hip4_sampler.devs[minor].type = OFFLINE;

		/* Update bit mask */
		set_bit(minor, bitmap_hip4_sampler_minor);
	}

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	hip4_collect_client.prv = mx;
	scsc_log_collector_register_client(&hip4_collect_client);
#endif
	hip4_sampler.init = true;

	SLSI_INFO_NODEV("%s: Ready to start sampling....\n", DRV_NAME);

	return;

error_class:
	unregister_chrdev_region(hip4_sampler.device, SCSC_HIP4_DEBUG_INTERFACES);
	hip4_sampler.init = false;
error:
	return;
}

void hip4_sampler_destroy(struct slsi_dev *sdev, struct scsc_mx *mx)
{
	int                     i = SCSC_HIP4_DEBUG_INTERFACES;
	struct hip4_sampler_dev *hip4_dev;

	while (i--)
		if (hip4_sampler.devs[i].cdev.dev && hip4_sampler.devs[i].mx) {
			hip4_dev = &hip4_sampler.devs[i];
			/* This should be never be true - as knod should prevent unloading while
			 * the service (device node) is open
			 */
			if (hip4_sampler.devs[i].filp) {
				hip4_sampler.devs[i].filp = NULL;
				kfifo_free(&hip4_sampler.devs[i].fifo);
			}
			if (hip4_sampler.devs[i].type == OFFLINE)
				kfifo_free(&hip4_sampler.devs[i].fifo);

			slsi_traffic_mon_client_unregister(sdev, hip4_dev);
			device_destroy(hip4_sampler.class_hip4_sampler, hip4_sampler.devs[i].cdev.dev);
			cdev_del(&hip4_sampler.devs[i].cdev);
			memset(&hip4_sampler.devs[i].cdev, 0, sizeof(struct cdev));
			hip4_sampler.devs[i].mx = NULL;
			clear_bit(i, bitmap_hip4_sampler_minor);
		}
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	scsc_log_collector_unregister_client(&hip4_collect_client);
#endif
	class_destroy(hip4_sampler.class_hip4_sampler);
	unregister_chrdev_region(hip4_sampler.device, SCSC_HIP4_DEBUG_INTERFACES);
	hip4_sampler.init = false;
}
