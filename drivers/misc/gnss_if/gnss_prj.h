/*
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __GNSS_PRJ_H__
#define __GNSS_PRJ_H__

#include <linux/wait.h>
#include <linux/miscdevice.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/wakelock.h>
#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include "include/gnss.h"
#include "include/exynos_ipc.h"
#include "pmu-gnss.h"

#define CALLER	(__builtin_return_address(0))

#define MAX_IOD_RXQ_LEN	2048

#define GNSS_IOC_MAGIC	('K')

#define GNSS_IOCTL_RESET			_IO(GNSS_IOC_MAGIC, 0x00)
#define GNSS_IOCTL_LOAD_FIRMWARE	_IO(GNSS_IOC_MAGIC, 0x01)
#define GNSS_IOCTL_REQ_FAULT_INFO	_IO(GNSS_IOC_MAGIC, 0x02)
#define GNSS_IOCTL_REQ_BCMD			_IO(GNSS_IOC_MAGIC, 0x03)
#define GNSS_IOCTL_READ_FIRMWARE	_IO(GNSS_IOC_MAGIC, 0x04)
#define GNSS_IOCTL_CHANGE_SENSOR_GPIO	_IO(GNSS_IOC_MAGIC, 0x05)
#define GNSS_IOCTL_CHANGE_TCXO_MODE		_IO(GNSS_IOC_MAGIC, 0x06)
#define GNSS_IOCTL_SET_SENSOR_POWER		_IO(GNSS_IOC_MAGIC, 0x07)
#define GNSS_IOCTL_PURE_RELEASE			_IO(GNSS_IOC_MAGIC, 0x50)

enum sensor_power {
	SENSOR_OFF,
	SENSOR_ON,
};

#define USE_SIMPLE_WAKE_LOCK

struct kepler_bcmd_args {
	u16 flags;
	u16 cmd_id;
	u32 param1;
	u32 param2;
	u32 ret_val;
};

struct kepler_firmware_args {
	u32 firmware_size;
	u32 offset;
	char *firmware_bin;
};

struct kepler_fault_args {
	u32 dump_size;
	char *dumped_data;
};

#ifdef CONFIG_COMPAT
struct kepler_firmware_args32 {
	u32 firmware_size;
	u32 offset;
	compat_uptr_t firmware_bin;
};

struct kepler_fault_args32 {
	u32 dump_size;
	compat_uptr_t dumped_data;
};
#endif

/* gnss status */
#define HDLC_HEADER_MAX_SIZE	6 /* fmt 3, raw 6, rfs 6 */

#define GNSS_MAX_NAME_LEN	64

#define MAX_HEX_LEN			16
#define MAX_NAME_LEN		64
#define MAX_PREFIX_LEN		128
#define MAX_STR_LEN			256

/* Does gnss ctl structure will use state ? or status defined below ?*/
enum gnss_state {
	STATE_OFFLINE,
	STATE_FIRMWARE_DL, /* no firmware */
	STATE_ONLINE,
	STATE_HOLD_RESET,
	STATE_FAULT, /* ACTIVE/WDT */
};

static const char const *gnss_state_str[] = {
	[STATE_OFFLINE]			= "OFFLINE",
	[STATE_FIRMWARE_DL]		= "FIRMWARE_DL",
	[STATE_ONLINE]			= "ONLINE",
	[STATE_HOLD_RESET]		= "HOLD_RESET",
	[STATE_FAULT]			= "FAULT",
};

enum direction {
	TX = 0,
	AP2GNSS = 0,
	RX = 1,
	GNSS2AP = 1,
	MAX_DIR = 2
};

/**
  @brief      return the gnss_state string
  @param state    the state of a GNSS
 */
static const inline char *get_gnss_state_str(int state)
{
	return gnss_state_str[state];
}

struct header_data {
	char hdr[HDLC_HEADER_MAX_SIZE];
	u32 len;
	u32 frag_len;
	char start; /*hdlc start header 0x7F*/
};

struct fmt_hdr {
	u16 len;
	u8 control;
} __packed;

/* for fragmented data from link devices */
struct fragmented_data {
	struct sk_buff *skb_recv;
	struct header_data h_data;
	struct exynos_frame_data f_data;
	/* page alloc fail retry*/
	unsigned realloc_offset;
};
#define fragdata(iod, ld) (&(iod)->fragments)

/** struct skbuff_priv - private data of struct sk_buff
 * this is matched to char cb[48] of struct sk_buff
 */
struct skbuff_private {
	struct io_device *iod;
	struct link_device *ld;
	struct io_device *real_iod; /* for rx multipdp */

	/* for time-stamping */
	struct timespec ts;

	u32 lnk_hdr:1,
		reserved:15,
		exynos_ch:8,
		frm_ctrl:8;

	/* for indicating that thers is only one IPC frame in an skb */
	bool single_frame;
} __packed;

static inline struct skbuff_private *skbpriv(struct sk_buff *skb)
{
	BUILD_BUG_ON(sizeof(struct skbuff_private) > sizeof(skb->cb));
	return (struct skbuff_private *)&skb->cb;
}

struct io_device {
	/* Name of the IO device */
	char *name;

	/* Link to link device */
	struct link_device *ld;

	/* Reference count */
	atomic_t opened;

	/* Wait queue for the IO device */
	wait_queue_head_t wq;

	/* Misc and net device structures for the IO device */
	struct miscdevice  miscdev;

	/* The name of the application that will use this IO device */
	char *app;

	bool link_header;

	/* Rx queue of sk_buff */
	struct sk_buff_head sk_rx_q;

	/*
	** work for each io device, when delayed work needed
	** use this for private io device rx action
	*/
	struct delayed_work rx_work;

	struct fragmented_data fragments;

	/* called from linkdevice when a packet arrives for this iodevice */
	int (*recv_skb)(struct io_device *iod, struct link_device *ld,
					struct sk_buff *skb);
	int (*recv_skb_single)(struct io_device *iod, struct link_device *ld,
					struct sk_buff *skb);

	struct gnss_ctl *gc;

	struct wake_lock wakelock;
	long waketime;

	struct exynos_seq_num seq_num;

	/* DO NOT use __current_link directly
	 * you MUST use skbpriv(skb)->ld in mc, link, etc..
	 */
	struct link_device *__current_link;
};
#define to_io_device(misc) container_of(misc, struct io_device, miscdev)

/* get_current_link, set_current_link don't need to use locks.
 * In ARM, set_current_link and get_current_link are compiled to
 * each one instruction (str, ldr) as atomic_set, atomic_read.
 * And, the order of set_current_link and get_current_link is not important.
 */
#define get_current_link(iod) ((iod)->__current_link)
#define set_current_link(iod, ld) ((iod)->__current_link = (ld))

struct link_device {
	struct list_head  list;
	char *name;

	/* GNSS data */
	struct gnss_data *gnss_data;

	/* GNSS control */
	struct gnss_ctl *gc;

	/* link to io device */
	struct io_device *iod;

	/* TX queue of socket buffers */
	struct sk_buff_head sk_fmt_tx_q;
	struct sk_buff_head *skb_txq;

	/* RX queue of socket buffers */
	struct sk_buff_head sk_fmt_rx_q;
	struct sk_buff_head *skb_rxq;

	int timeout_cnt;

	struct workqueue_struct *tx_wq;
	struct work_struct tx_work;
	struct delayed_work tx_delayed_work;

	struct delayed_work *tx_dwork;
	struct delayed_work fmt_tx_dwork;

	struct workqueue_struct *rx_wq;
	struct work_struct rx_work;
	struct delayed_work rx_delayed_work;

	/* called by an io_device when it has a packet to send over link
	 * - the io device is passed so the link device can look at id and
	 *   format fields to determine how to route/format the packet
	 */
	int (*send)(struct link_device *ld, struct io_device *iod,
			struct sk_buff *skb);

	/* Method to clear RX/TX buffers before reset */
	void (*reset_buffers)(struct link_device *ld);

	/* Methods for copying to/from reserved memory */
	int (*copy_reserved_from_user)(struct link_device *ld, u32 offset, \
					void __user *user_src, u32 size);
	int (*copy_reserved_to_user)(struct link_device *ld, u32 offset, \
					void __user *user_dst, u32 size);

	/* Method to dump fault info to user */
	int (*dump_fault_to_user)(struct link_device *ld, \
					void __user *user_dst, u32 size);
};

/** rx_alloc_skb - allocate an skbuff and set skb's iod, ld
 * @length:	length to allocate
 * @iod:	struct io_device *
 * @ld:		struct link_device *
 *
 * %NULL is returned if there is no free memory.
 */
static inline struct sk_buff *rx_alloc_skb(unsigned int length,
		struct io_device *iod, struct link_device *ld)
{
	struct sk_buff *skb;

	skb = alloc_skb(length, GFP_ATOMIC);

	if (likely(skb)) {
		skbpriv(skb)->iod = iod;
		skbpriv(skb)->ld = ld;
	}
	return skb;
}

enum gnss_mode;
enum gnss_int_clear;
enum gnss_tcxo_mode;

struct gnssctl_ops {
	int (*gnss_hold_reset)(struct gnss_ctl *);
	int (*gnss_release_reset)(struct gnss_ctl *);
	int (*gnss_power_on)(struct gnss_ctl *);
	int (*gnss_req_fault_info)(struct gnss_ctl *);
	int (*suspend_gnss_ctrl)(struct gnss_ctl *);
	int (*resume_gnss_ctrl)(struct gnss_ctl *);
	int (*change_sensor_gpio)(struct gnss_ctl *);
	int (*set_sensor_power)(struct gnss_ctl *, enum sensor_power);
	int (*req_bcmd)(struct gnss_ctl *, u16, u16, u32, u32);
	int (*gnss_pure_release)(struct gnss_ctl *);
};

struct gnss_ctl {
	struct device *dev;
	char *name;
	struct gnss_data *gnss_data;
	enum gnss_state gnss_state;

	struct clk *ccore_qch_lh_gnss;

	struct delayed_work dwork;
	struct work_struct work;

	struct gnssctl_ops ops;
	struct gnssctl_pmu_ops *pmu_ops;
	struct io_device *iod;

	/* Wakelock for gnss_ctl */
	struct wake_lock gc_fault_wake_lock;
	struct wake_lock gc_wake_lock;
	struct wake_lock gc_bcmd_wake_lock;

	int wake_lock_irq;
	int req_init_irq;
	struct completion fault_cmpl;
	struct completion bcmd_cmpl;
	struct completion req_init_cmpl;

	struct pinctrl *gnss_gpio;
	struct pinctrl_state *gnss_sensor_gpio;

	struct regulator *vdd_sensor_reg;
};

extern int exynos_init_gnss_io_device(struct io_device *iod);

int init_gnssctl_device(struct gnss_ctl *mc, struct gnss_data *pdata);
struct link_device *create_link_device_shmem(struct platform_device *pdev);

#endif
