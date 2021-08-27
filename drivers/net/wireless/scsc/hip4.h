/****************************************************************************
 *
 * Copyright (c) 2014 - 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __HIP4_H__
#define __HIP4_H__

/**
 * This header file is the public HIP4 interface, which will be accessible by
 * Wi-Fi service driver components.
 *
 * All struct and internal HIP functions shall be moved to a private header
 * file.
 */

#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/skbuff.h>
#include <scsc/scsc_mifram.h>
#include <scsc/scsc_mx.h>
#ifdef CONFIG_SCSC_WLAN_RX_NAPI
#include <linux/netdevice.h>
#endif
#ifdef CONFIG_SCSC_WLAN_ANDROID
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <scsc/scsc_wakelock.h>
#else
#include <linux/wakelock.h>
#endif
#endif
#include "mbulk.h"
#ifdef CONFIG_SCSC_SMAPPER
#include "hip4_smapper.h"
#endif

/* Shared memory Layout
 *
 * |-------------------------| CONFIG
 * |    CONFIG  + Queues     |
 * |       ---------         |
 * |          MIB            |
 * |-------------------------| TX Pool
 * |         TX DAT          |
 * |       ---------         |
 * |         TX CTL          |
 * |-------------------------| RX Pool
 * |          RX             |
 * |-------------------------|
 */

/**** OFFSET SHOULD BE 4096 BYTES ALIGNED ***/
/*** CONFIG POOL ***/
#define HIP4_WLAN_CONFIG_OFFSET	0x00000
#define HIP4_WLAN_CONFIG_SIZE	0x02000 /* 8 kB */
/*** MIB POOL ***/
#define HIP4_WLAN_MIB_OFFSET	(HIP4_WLAN_CONFIG_OFFSET +  HIP4_WLAN_CONFIG_SIZE)
#define HIP4_WLAN_MIB_SIZE	0x08000 /* 32 kB */
/*** TX POOL ***/
#define HIP4_WLAN_TX_OFFSET	(HIP4_WLAN_MIB_OFFSET + HIP4_WLAN_MIB_SIZE)
/*** TX POOL - DAT POOL ***/
#define HIP4_WLAN_TX_DAT_OFFSET	HIP4_WLAN_TX_OFFSET
#define HIP4_WLAN_TX_DAT_SIZE	0x100000 /* 1 MB */
/*** TX POOL - CTL POOL ***/
#define HIP4_WLAN_TX_CTL_OFFSET	(HIP4_WLAN_TX_DAT_OFFSET + HIP4_WLAN_TX_DAT_SIZE)
#define HIP4_WLAN_TX_CTL_SIZE	0x10000 /*  64 kB */
#define HIP4_WLAN_TX_SIZE	(HIP4_WLAN_TX_DAT_SIZE + HIP4_WLAN_TX_CTL_SIZE)
/*** RX POOL ***/
#define HIP4_WLAN_RX_OFFSET	(HIP4_WLAN_TX_CTL_OFFSET +  HIP4_WLAN_TX_CTL_SIZE)
#ifdef CONFIG_SCSC_PCIE
#define HIP4_WLAN_RX_SIZE	0x80000  /* 512 kB */
#else
#define HIP4_WLAN_RX_SIZE	0x100000 /* 1 MB */
#endif
/*** TOTAL : CONFIG POOL + TX POOL + RX POOL ***/
#define HIP4_WLAN_TOTAL_MEM	(HIP4_WLAN_CONFIG_SIZE + HIP4_WLAN_MIB_SIZE + \
				 HIP4_WLAN_TX_SIZE + HIP4_WLAN_RX_SIZE) /* 2 MB + 104 KB*/

#define HIP4_POLLING_MAX_PACKETS 512

#define HIP4_DAT_MBULK_SIZE	(2 * 1024)
#define HIP4_DAT_SLOTS		(HIP4_WLAN_TX_DAT_SIZE / HIP4_DAT_MBULK_SIZE)
#define HIP4_CTL_MBULK_SIZE	(2 * 1024)
#define HIP4_CTL_SLOTS		(HIP4_WLAN_TX_CTL_SIZE / HIP4_CTL_MBULK_SIZE)

#define MIF_HIP_CFG_Q_NUM       6

#define MIF_NO_IRQ		0xff

/* Current versions supported by this HIP */
#define HIP4_SUPPORTED_V1	3
#define HIP4_SUPPORTED_V2	4

enum hip4_hip_q_conf {
	HIP4_MIF_Q_FH_CTRL,
	HIP4_MIF_Q_FH_DAT,
	HIP4_MIF_Q_FH_RFB,
	HIP4_MIF_Q_TH_CTRL,
	HIP4_MIF_Q_TH_DAT,
	HIP4_MIF_Q_TH_RFB
};

struct hip4_hip_config_version_4 {
	/* Host owned */
	u32 magic_number;       /* 0xcaba0401 */
	u16 hip_config_ver;     /* Version of this configuration structure = 2*/
	u16 config_len;         /* Size of this configuration structure */

	/* FW owned */
	u32 compat_flag;         /* flag of the expected driver's behaviours */

	u16 sap_mlme_ver;        /* Fapi SAP_MLME version*/
	u16 sap_ma_ver;          /* Fapi SAP_MA version */
	u16 sap_debug_ver;       /* Fapi SAP_DEBUG version */
	u16 sap_test_ver;        /* Fapi SAP_TEST version */

	u32 fw_build_id;         /* Firmware Build Id */
	u32 fw_patch_id;         /* Firmware Patch Id */

	u8  unidat_req_headroom; /* Headroom the host shall reserve in mbulk for MA-UNITDATA.REQ signal */
	u8  unidat_req_tailroom; /* Tailroom the host shall reserve in mbulk for MA-UNITDATA.REQ signal */
	u8  bulk_buffer_align;   /* 4 */

	/* Host owned */
	u8  host_cache_line;    /* 64 */

	u32 host_buf_loc;       /* location of the host buffer in MIF_ADDR */
	u32 host_buf_sz;        /* in byte, size of the host buffer */
	u32 fw_buf_loc;         /* location of the firmware buffer in MIF_ADDR */
	u32 fw_buf_sz;          /* in byte, size of the firmware buffer */
	u32 mib_loc;            /* MIB location in MIF_ADDR */
	u32 mib_sz;             /* MIB size */
	u32 log_config_loc;     /* Logging Configuration Location in MIF_ADDR */
	u32 log_config_sz;      /* Logging Configuration Size in MIF_ADDR */

	u8 mif_fh_int_n;		/* MIF from-host interrupt bit position for all HIP queue */
	u8 reserved1[3];

	u8 mif_th_int_n[6];		/* MIF to-host interrupt bit positions for each HIP queue */
	u8 reserved2[2];

	u32 scbrd_loc;          /* Scoreboard locatin in MIF_ADDR */

	u16 q_num;              /* 6 */
	u16 q_len;              /* 256 */
	u16 q_idx_sz;           /* 1 */
	u8  reserved3[2];

	u32 q_loc[MIF_HIP_CFG_Q_NUM];

#ifdef CONFIG_SCSC_SMAPPER
	u8  smapper_th_req;     /* TH smapper request interrupt bit position */
	u8  smapper_fh_ind;     /* FH smapper ind interrupt bit position */
	u8  smapper_mbox_scb;   /* SMAPPER MBOX scoreboard location */
	u8  smapper_entries_banks[16];  /* num entries banks */
	u8  smapper_pow_sz[16];     /* Power of size of entry i.e. 12 = 4096B */
	u32 smapper_bank_addr[16]; /* Bank start addr */
#else
	u8  reserved_nosmapper[99];
#endif
	u8  reserved4[16];
} __packed;

struct hip4_hip_config_version_5 {
	/* Host owned */
	u32 magic_number;       /* 0xcaba0401 */
	u16 hip_config_ver;     /* Version of this configuration structure = 2*/
	u16 config_len;         /* Size of this configuration structure */

	/* FW owned */
	u32 compat_flag;         /* flag of the expected driver's behaviours */

	u16 sap_mlme_ver;        /* Fapi SAP_MLME version*/
	u16 sap_ma_ver;          /* Fapi SAP_MA version */
	u16 sap_debug_ver;       /* Fapi SAP_DEBUG version */
	u16 sap_test_ver;        /* Fapi SAP_TEST version */

	u32 fw_build_id;         /* Firmware Build Id */
	u32 fw_patch_id;         /* Firmware Patch Id */

	u8  unidat_req_headroom; /* Headroom the host shall reserve in mbulk for MA-UNITDATA.REQ signal */
	u8  unidat_req_tailroom; /* Tailroom the host shall reserve in mbulk for MA-UNITDATA.REQ signal */
	u8  bulk_buffer_align;   /* 4 */

	/* Host owned */
	u8  host_cache_line;    /* 64 */

	u32 host_buf_loc;       /* location of the host buffer in MIF_ADDR */
	u32 host_buf_sz;        /* in byte, size of the host buffer */
	u32 fw_buf_loc;         /* location of the firmware buffer in MIF_ADDR */
	u32 fw_buf_sz;          /* in byte, size of the firmware buffer */
	u32 mib_loc;            /* MIB location in MIF_ADDR */
	u32 mib_sz;             /* MIB size */
	u32 log_config_loc;     /* Logging Configuration Location in MIF_ADDR */
	u32 log_config_sz;      /* Logging Configuration Size in MIF_ADDR */

	u8  mif_fh_int_n;       /* MIF from-host interrupt bit position */
	u8  mif_th_int_n;       /* MIF to-host interrpt bit position */
	u8  reserved[2];

	u32 scbrd_loc;          /* Scoreboard locatin in MIF_ADDR */

	u16 q_num;              /* 6 */
	u16 q_len;              /* 256 */
	u16 q_idx_sz;           /* 1 */
	u8  reserved2[2];

	u32 q_loc[MIF_HIP_CFG_Q_NUM];

	u8  reserved3[16];
} __packed;

struct hip4_hip_init {
	/* Host owned */
	u32 magic_number;       /* 0xcaaa0400 */
	/* FW owned */
	u32 conf_hip4_ver;
	/* Host owned */
	u32 version_a_ref;      /* Location of Config structure A (old) */
	u32 version_b_ref;      /* Location of Config structure B (new) */
} __packed;

#define MAX_NUM 256
struct hip4_hip_q {
	u32 array[MAX_NUM];
	u8  idx_read;      /* To keep track */
	u8  idx_write;     /* To keep track */
	u8  total;
} __aligned(64);

struct hip4_hip_control {
	struct hip4_hip_init             init;
	struct hip4_hip_config_version_5 config_v5 __aligned(32);
	struct hip4_hip_config_version_4 config_v4 __aligned(32);
	u32                              scoreboard[256] __aligned(64);
	struct hip4_hip_q                q[MIF_HIP_CFG_Q_NUM] __aligned(64);
} __aligned(4096);

struct slsi_hip4;

/* This struct is private to the HIP implementation */
struct hip4_priv {
#ifdef CONFIG_SCSC_WLAN_RX_NAPI
	/* NAPI CPU switch lock */
	spinlock_t                   napi_cpu_lock;
	struct work_struct           intr_wq_napi_cpu_switch;
	struct work_struct           intr_wq_ctrl;
	struct tasklet_struct	     intr_tl_fb;
	struct napi_struct           napi;
	unsigned long                napi_state;
#else
	struct work_struct           intr_wq;
#endif
	/* Interrupts cache < v4 */
	/* TOHOST */
	u32                          intr_tohost;

	/* Interrupts cache v4 */
	u32                          intr_tohost_mul[MIF_HIP_CFG_Q_NUM];
	/* FROMHOST */
	u32                          intr_fromhost;

	/* For workqueue */
	struct slsi_hip4             *hip;

	/* Pool for data frames*/
	u8                           host_pool_id_dat;
	/* Pool for ctl frames*/
	u8                           host_pool_id_ctl;

	/* rx cycle lock */
	spinlock_t                   rx_lock;
	/* tx cycle lock */
	spinlock_t                   tx_lock;

	/* Scoreboard update spinlock */
	rwlock_t                     rw_scoreboard;

	/* Watchdog timer */
	struct timer_list            watchdog;
	/* wd spinlock */
	spinlock_t                   watchdog_lock;
	/* wd timer control */
	atomic_t                     watchdog_timer_active;
#ifdef CONFIG_SCSC_WLAN_RX_NAPI
	DECLARE_BITMAP(irq_bitmap, MIF_HIP_CFG_Q_NUM);
#endif

#ifdef CONFIG_SCSC_WLAN_ANDROID
#ifdef CONFIG_SCSC_WLAN_RX_NAPI
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	struct scsc_wake_lock             hip4_wake_lock_tx;
	struct scsc_wake_lock             hip4_wake_lock_ctrl;
	struct scsc_wake_lock             hip4_wake_lock_data;
#else

	struct wake_lock             hip4_wake_lock_tx;
	struct wake_lock             hip4_wake_lock_ctrl;
	struct wake_lock             hip4_wake_lock_data;
#endif
#endif
	/* Wakelock for modem_ctl */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	struct scsc_wake_lock             hip4_wake_lock;
#else
	struct wake_lock             hip4_wake_lock;
#endif
#endif

	/* Control the hip4 deinit */
	atomic_t                     closing;
	atomic_t                     in_tx;
	atomic_t                     in_rx;
	atomic_t                     in_suspend;
	u32                          storm_count;

	struct {
		atomic_t	     irqs;
		atomic_t	     spurious_irqs;
		u32 q_num_frames[MIF_HIP_CFG_Q_NUM];
		ktime_t start;
		struct proc_dir_entry   *procfs_dir;
	} stats;

#ifdef CONFIG_SCSC_WLAN_HIP4_PROFILING
	/*minor*/
	u32                          minor;
#endif
	u8                           unidat_req_headroom; /* Headroom the host shall reserve in mbulk for MA-UNITDATA.REQ signal */
	u8                           unidat_req_tailroom; /* Tailroom the host shall reserve in mbulk for MA-UNITDATA.REQ signal */
	u32                          version; /* Version of the running FW */
	void                         *scbrd_base; /* Scbrd_base pointer */

	/* Global domain Q control*/
	atomic_t                     gactive;
	atomic_t                     gmod;
	atomic_t                     gcod;
	int                          saturated;
	int                          guard;
	/* Global domain Q spinlock */
	spinlock_t                   gbot_lock;

#ifdef CONFIG_SCSC_SMAPPER
	/* SMAPPER */
	/*  Leman has 4 Banks of 160 entries each and 4 Banks of 64 entries each. Each Tx stream is
	 *  expected to use 2 Bank . In RSDB, 5GHz streams require higher throughput
	 *  so the bigger banks are allocated for 5GHz streams and the
	 *  smaller banks are for 2.4GHz streams
	 */
	struct hip4_smapper_bank     smapper_banks[HIP4_SMAPPER_TOTAL_BANKS];
	struct hip4_smapper_control  smapper_control;
#endif
#ifdef CONFIG_SCSC_QOS
	/* PM QoS control */
	struct work_struct           pm_qos_work;
	/* PM QoS control spinlock */
	spinlock_t                   pm_qos_lock;
	u8                           pm_qos_state;
#endif
	/* Collection artificats */
	void                         *mib_collect;
	u16                          mib_sz;
	/* Mutex to protect hcf file collection if a tear down is triggered */
	struct mutex                 in_collection;

	struct workqueue_struct *hip4_workq;
};

struct scsc_service;

struct slsi_hip4 {
	struct hip4_priv        *hip_priv;
	struct hip4_hip_control *hip_control;
	scsc_mifram_ref         hip_ref;
};

/* Public functions */
int hip4_init(struct slsi_hip4 *hip);
int hip4_setup(struct slsi_hip4 *hip);
void hip4_suspend(struct slsi_hip4 *hip);
void hip4_resume(struct slsi_hip4 *hip);
void hip4_freeze(struct slsi_hip4 *hip);
void hip4_deinit(struct slsi_hip4 *hip);
int hip4_free_ctrl_slots_count(struct slsi_hip4 *hip);
void hip4_set_napi_cpu(struct slsi_hip4 *hip, u8 napi_cpu);
int scsc_wifi_transmit_frame(struct slsi_hip4 *hip, struct sk_buff *skb, bool ctrl_packet, u8 vif_index, u8 peer_index, u8 priority);
#ifndef CONFIG_SCSC_WLAN_RX_NAPI
void hip4_sched_wq(struct slsi_hip4 *hip);
#endif

/* Macros for accessing information stored in the hip_config struct */
#define scsc_wifi_get_hip_config_version_4_u8(buff_ptr, member) le16_to_cpu((((struct hip4_hip_config_version_4 *)(buff_ptr))->member))
#define scsc_wifi_get_hip_config_version_4_u16(buff_ptr, member) le16_to_cpu((((struct hip4_hip_config_version_4 *)(buff_ptr))->member))
#define scsc_wifi_get_hip_config_version_4_u32(buff_ptr, member) le32_to_cpu((((struct hip4_hip_config_version_4 *)(buff_ptr))->member))
#define scsc_wifi_get_hip_config_version_5_u8(buff_ptr, member) le16_to_cpu((((struct hip4_hip_config_version_5 *)(buff_ptr))->member))
#define scsc_wifi_get_hip_config_version_5_u16(buff_ptr, member) le16_to_cpu((((struct hip4_hip_config_version_5 *)(buff_ptr))->member))
#define scsc_wifi_get_hip_config_version_5_u32(buff_ptr, member) le32_to_cpu((((struct hip4_hip_config_version_5 *)(buff_ptr))->member))
#define scsc_wifi_get_hip_config_u8(buff_ptr, member, ver) le16_to_cpu((((struct hip4_hip_config_version_##ver *)(buff_ptr->config_v##ver))->member))
#define scsc_wifi_get_hip_config_u16(buff_ptr, member, ver) le16_to_cpu((((struct hip4_hip_config_version_##ver *)(buff_ptr->config_v##ver))->member))
#define scsc_wifi_get_hip_config_u32(buff_ptr, member, ver) le32_to_cpu((((struct hip4_hip_config_version_##ver *)(buff_ptr->config_v##ver))->member))
#define scsc_wifi_get_hip_config_version(buff_ptr) le32_to_cpu((((struct hip4_hip_init *)(buff_ptr))->conf_hip4_ver))

#endif
