/****************************************************************************
 *
 * Copyright (c) 2012 - 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <linux/kernel.h>
#include "dev.h"
#include <scsc/scsc_logring.h>

/* Logging modules
 * =======================
 */
#ifdef CONFIG_SCSC_WLAN_DEBUG
#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif
#else
#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[4], (a)[5]
#define MACSTR "%02x:**:**:**:%02x:%02x"
#endif
#endif

#ifndef CONFIG_SCSC_DEBUG_COMPATIBILITY
extern const int SLSI_INIT_DEINIT;
extern const int SLSI_NETDEV;
extern const int SLSI_CFG80211;
extern const int SLSI_MLME;
extern const int SLSI_SUMMARY_FRAMES;
extern const int SLSI_HYDRA;
extern const int SLSI_TX;
extern const int SLSI_RX;
extern const int SLSI_UDI;

extern const int SLSI_WIFI_FCQ;

extern const int SLSI_HIP;
extern const int SLSI_HIP_INIT_DEINIT;
extern const int SLSI_HIP_FW_DL;
extern const int SLSI_HIP_SDIO_OP;
extern const int SLSI_HIP_PS;
extern const int SLSI_HIP_TH;
extern const int SLSI_HIP_FH;
extern const int SLSI_HIP_SIG;

extern const int SLSI_FUNC_TRACE;
extern const int SLSI_TEST;
extern const int SLSI_SRC_SINK;
extern const int SLSI_FW_TEST;
extern const int SLSI_RX_BA;

extern const int SLSI_TDLS;
extern const int SLSI_GSCAN;
extern const int SLSI_MBULK;
extern const int SLSI_FLOWC;
extern const int SLSI_SMAPPER;
#endif /* CONFIG_SCSC_DEBUG_COMPATIBILITY */

extern int       *slsi_dbg_filters[];

#ifndef pr_warn
#define pr_warn pr_warning
#endif

/*---------------------------*/

/**
 * debug logging functions
 * =======================
 */

#define SLSI_EWI_NODEV_LABEL "ieee80211 phy.: "
#define SLSI_EWI_DEV(sdev) (likely((sdev) && ((sdev)->wiphy)) ? &((sdev)->wiphy->dev) : NULL)
#define SLSI_EWI_NET_DEV(ndev) (likely(ndev) ? SLSI_EWI_DEV(((struct netdev_vif *)netdev_priv(ndev))->sdev) : NULL)
#define SLSI_EWI_NET_NAME(ndev) (likely(ndev) ? netdev_name(ndev) : NULL)

#define SLSI_EWI(output, sdev, label, fmt, arg ...)     output(SLSI_EWI_DEV(sdev), SCSC_PREFIX label ": %s: " fmt, __func__, ## arg)
#define SLSI_EWI_NET(output, ndev, label, fmt, arg ...) output(SLSI_EWI_NET_DEV(ndev), SCSC_PREFIX "%s: " label ": %s: " fmt, SLSI_EWI_NET_NAME(ndev), __func__, ## arg)
#define SLSI_EWI_NODEV(output, label, fmt, arg ...)     output(SLSI_EWI_NODEV_LABEL SCSC_PREFIX label ": %s: " fmt, __func__, ## arg)

#define SLSI_EWI_HEX(output, klevel, sdev, label, p, len, fmt, arg ...) \
	do { \
		SLSI_EWI(output, sdev, label, fmt, ## arg); \
		print_hex_dump(klevel, SCSC_PREFIX, DUMP_PREFIX_OFFSET, 16, 1, p, len, 0); \
	} while (0)

#define SLSI_EWI_HEX_NET(output, klevel, dev, label, p, len, fmt, arg ...) \
	do { \
		SLSI_EWI_NET(output, dev, label, fmt, ## arg); \
		print_hex_dump(klevel, SCSC_PREFIX, DUMP_PREFIX_OFFSET, 16, 1, p, len, 0); \
	} while (0)

#define SLSI_EWI_HEX_NODEV(output, klevel, label, p, len, fmt, arg ...) \
	do { \
		SLSI_EWI_NODEV(output, label, fmt, ## arg); \
		print_hex_dump(klevel, SCSC_PREFIX, DUMP_PREFIX_OFFSET, 16, 1, p, len, 0);  \
	} while (0)

#define SLSI_ERR(sdev, fmt, arg ...)                SLSI_EWI(dev_err,  sdev, "E", fmt, ## arg)
#define SLSI_WARN(sdev, fmt, arg ...)               SLSI_EWI(dev_warn, sdev, "W", fmt, ## arg)
#define SLSI_INFO(sdev, fmt, arg ...)               SLSI_EWI(dev_info, sdev, "I", fmt, ## arg)

#define SLSI_NET_ERR(ndev, fmt, arg ...)            SLSI_EWI_NET(dev_err,  ndev, "E", fmt, ## arg)
#define SLSI_NET_WARN(ndev, fmt, arg ...)           SLSI_EWI_NET(dev_warn, ndev, "W", fmt, ## arg)
#define SLSI_NET_INFO(ndev, fmt, arg ...)           SLSI_EWI_NET(dev_info, ndev, "I", fmt, ## arg)

#define SLSI_ERR_NODEV(fmt, arg ...)                SLSI_EWI_NODEV(pr_err,  "E", fmt, ## arg)
#define SLSI_WARN_NODEV(fmt, arg ...)               SLSI_EWI_NODEV(pr_warn, "W", fmt, ## arg)
#define SLSI_INFO_NODEV(fmt, arg ...)               SLSI_EWI_NODEV(pr_info, "I", fmt, ## arg)

#define SLSI_ERR_HEX(sdev, p, len, fmt, arg ...)    SLSI_EWI_HEX(dev_err,  KERN_ERR, sdev,  "E", p, len, fmt, ## arg)
#define SLSI_WARN_HEX(sdev, p, len, fmt, arg ...)   SLSI_EWI_HEX(dev_warn, KERN_WARN, sdev, "W", p, len, fmt, ## arg)
#define SLSI_INFO_HEX(sdev, p, len, fmt, arg ...)   SLSI_EWI_HEX(dev_info, KERN_INFO, sdev, "I", p, len, fmt, ## arg)

#define SLSI_ERR_HEX_NODEV(p, len, fmt, arg ...)    SLSI_EWI_HEX_NODEV(pr_err,  KERN_ERR,  "E", p, len, fmt, ## arg)
#define SLSI_WARN_HEX_NODEV(p, len, fmt, arg ...)   SLSI_EWI_HEX_NODEV(pr_warn, KERN_WARN, "W", p, len, fmt, ## arg)
#define SLSI_INFO_HEX_NODEV(p, len, fmt, arg ...)   SLSI_EWI_HEX_NODEV(pr_info, KERN_INFO, "I", p, len, fmt, ## arg)

#ifdef CONFIG_SCSC_WLAN_DEBUG

#define SLSI_DBG(sdev, filter, dbg_lvl, fmt, arg ...) \
	do { \
		if (unlikely((dbg_lvl) <= *slsi_dbg_filters[filter])) { \
			SLSI_EWI(dev_info, sdev, # dbg_lvl, fmt, ## arg); \
		} \
	} while (0)

#define SLSI_DBG_NET(ndev, filter, dbg_lvl, fmt, arg ...) \
	do { \
		if (unlikely((dbg_lvl) <= *slsi_dbg_filters[filter])) { \
			SLSI_EWI_NET(dev_info, ndev, # dbg_lvl, fmt, ## arg); \
		} \
	} while (0)

#define SLSI_DBG_NODEV(filter, dbg_lvl, fmt, arg ...) \
	do { \
		if (unlikely((dbg_lvl) <= *slsi_dbg_filters[filter])) { \
			SLSI_EWI_NODEV(pr_info, # dbg_lvl, fmt, ## arg); \
		} \
	} while (0)

#define SLSI_DBG1(sdev, filter, fmt, arg ...)   SLSI_DBG(sdev, filter, 1, fmt, ## arg)
#define SLSI_DBG2(sdev, filter, fmt, arg ...)   SLSI_DBG(sdev, filter, 2, fmt, ## arg)
#define SLSI_DBG3(sdev, filter, fmt, arg ...)   SLSI_DBG(sdev, filter, 3, fmt, ## arg)
#define SLSI_DBG4(sdev, filter, fmt, arg ...)   SLSI_DBG(sdev, filter, 4, fmt, ## arg)

#define SLSI_NET_DBG1(ndev, filter, fmt, arg ...)   SLSI_DBG_NET(ndev, filter, 1, fmt, ## arg)
#define SLSI_NET_DBG2(ndev, filter, fmt, arg ...)   SLSI_DBG_NET(ndev, filter, 2, fmt, ## arg)
#define SLSI_NET_DBG3(ndev, filter, fmt, arg ...)   SLSI_DBG_NET(ndev, filter, 3, fmt, ## arg)
#define SLSI_NET_DBG4(ndev, filter, fmt, arg ...)   SLSI_DBG_NET(ndev, filter, 4, fmt, ## arg)

#define SLSI_DBG1_NODEV(filter, fmt, arg ...)    SLSI_DBG_NODEV(filter, 1, fmt, ## arg)
#define SLSI_DBG2_NODEV(filter, fmt, arg ...)    SLSI_DBG_NODEV(filter, 2, fmt, ## arg)
#define SLSI_DBG3_NODEV(filter, fmt, arg ...)    SLSI_DBG_NODEV(filter, 3, fmt, ## arg)
#define SLSI_DBG4_NODEV(filter, fmt, arg ...)    SLSI_DBG_NODEV(filter, 4, fmt, ## arg)

/* Prints LOG_ENTRY if the condition evaluates to TRUE otherwise nothing is printed. */
#define LOG_CONDITIONALLY(condition, LOG_ENTRY) \
	do { \
		if (unlikely(condition)) \
			LOG_ENTRY; \
	} while (0)

/* Returns TRUE if the flag is set otherwise returns FALSE. */
#define LOG_BOOL_FLAG(flag) \
	(flag) ? "TRUE" : "FALSE"

#define SLSI_DBG_HEX_OUT(sdev, filter, dbg_lvl, p, len, fmt, arg ...) \
	do { \
		if (unlikely((dbg_lvl) <= *slsi_dbg_filters[filter])) { \
			SLSI_EWI_HEX(dev_info, KERN_INFO, sdev, # dbg_lvl, p, len, fmt, ## arg); \
		} \
	} while (0)

#define SLSI_DBG_HEX_OUT_NET(sdev, filter, dbg_lvl, p, len, fmt, arg ...) \
	do { \
		if (unlikely((dbg_lvl) <= *slsi_dbg_filters[filter])) { \
			SLSI_EWI_HEX_NET(dev_info, KERN_INFO, dev, # dbg_lvl, p, len, fmt, ## arg); \
		} \
	} while (0)

#define SLSI_DBG_HEX_OUT_NODEV(filter, dbg_lvl, p, len, fmt, arg ...) \
	do { \
		if (unlikely((dbg_lvl) <= *slsi_dbg_filters[filter])) { \
			SLSI_EWI_HEX_NODEV(pr_info, KERN_INFO, # dbg_lvl, p, len, fmt, ## arg); \
		} \
	} while (0)

#define SLSI_DBG_HEX(sdev, filter, p, len, fmt, arg ...)  SLSI_DBG_HEX_OUT(sdev, filter, 4, p, len, fmt, ## arg)
#define SLSI_NET_DBG_HEX(dev, filter, p, len, fmt, arg ...)  SLSI_DBG_HEX_OUT_NET(dev, filter, 4, p, len, fmt, ## arg)
#define SLSI_DBG_HEX_NODEV(filter, p, len, fmt, arg ...)   SLSI_DBG_HEX_OUT_NODEV(filter, 4, p, len, fmt, ## arg)

#define FUNC_ENTER(sdev) SLSI_DBG4(sdev, SLSI_FUNC_TRACE, "--->\n")
#define FUNC_EXIT(sdev)  SLSI_DBG4(sdev, SLSI_FUNC_TRACE, "<---\n")

#define FUNC_ENTER_NODEV() SLSI_DBG4_NODEV(SLSI_FUNC_TRACE, "--->\n")
#define FUNC_EXIT_NODEV()  SLSI_DBG4_NODEV(SLSI_FUNC_TRACE, "<---\n")

void slsi_debug_frame(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb, const char *prefix);
#else /* CONFIG_SCSC_WLAN_DEBUG */

#define SLSI_DBG1(sdev, filter, fmt, arg ...)   do {} while (0)
#define SLSI_DBG2(sdev, filter, fmt, arg ...)   do {} while (0)
#define SLSI_DBG3(sdev, filter, fmt, arg ...)   do {} while (0)
#define SLSI_DBG4(sdev, filter, fmt, arg ...)   do {} while (0)
#define SLSI_NET_DBG1(dev, filter, fmt, arg ...)        do {} while (0)
#define SLSI_NET_DBG2(dev, filter, fmt, arg ...)        do {} while (0)
#define SLSI_NET_DBG3(dev, filter, fmt, arg ...)        do {} while (0)
#define SLSI_NET_DBG4(dev, filter, fmt, arg ...)        do {} while (0)
#define SLSI_DBG1_NODEV(filter, fmt, arg ...)           do {} while (0)
#define SLSI_DBG2_NODEV(filter, fmt, arg ...)           do {} while (0)
#define SLSI_DBG3_NODEV(filter, fmt, arg ...)           do {} while (0)
#define SLSI_DBG4_NODEV(filter, fmt, arg ...)           do {} while (0)

#define LOG_CONDITIONALLY(condition, LOG_ENTRY)         do {} while (0)
#define LOG_BOOL_FLAG(flag)                             do {} while (0)

#define SLSI_DBG_HEX(sdev, filter, p, len, fmt, arg ...)        do {} while (0)
#define SLSI_NET_DBG_HEX(dev, filter, p, len, fmt, arg ...)     do {} while (0)
#define SLSI_DBG_HEX_NODEV(filter, p, len, fmt, arg ...)        do {} while (0)

#define FUNC_ENTER(sdev)
#define FUNC_EXIT(sdev)

#define FUNC_ENTER_NODEV()
#define FUNC_EXIT_NODEV()

static inline void slsi_debug_frame(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb, const char *prefix)
{
	(void)sdev;   /* unused */
	(void)dev;    /* unused */
	(void)skb;    /* unused */
	(void)prefix; /* unused */
}

#endif /* CONFIG_SCSC_WLAN_DEBUG */

#ifdef CONFIG_SCSC_DEBUG_COMPATIBILITY

#undef SLSI_ERR
#undef SLSI_WARN
#undef SLSI_INFO
#define SLSI_ERR(sdev, fmt, arg ...)		SCSC_ERR_SDEV(sdev, fmt, ## arg)
#define SLSI_WARN(sdev, fmt, arg ...)           SCSC_WARNING_SDEV(sdev, fmt, ## arg)
#define SLSI_INFO(sdev, fmt, arg ...)		SCSC_INFO_SDEV(sdev, fmt, ## arg)

#undef SLSI_NET_ERR
#undef SLSI_NET_WARN
#undef SLSI_NET_INFO
#define SLSI_NET_ERR(ndev, fmt, arg ...)        SCSC_ERR_NDEV(ndev, fmt, ## arg)
#define SLSI_NET_WARN(ndev, fmt, arg ...)       SCSC_WARNING_NDEV(ndev, fmt, ## arg)
#define SLSI_NET_INFO(ndev, fmt, arg ...)       SCSC_INFO_NDEV(ndev, fmt, ## arg)

#undef SLSI_ERR_NODEV
#undef SLSI_WARN_NODEV
#undef SLSI_INFO_NODEV
#define SLSI_ERR_NODEV(fmt, arg ...)            SCSC_ERR(fmt, ## arg)
#define SLSI_WARN_NODEV(fmt, arg ...)           SCSC_WARNING(fmt, ## arg)
#define SLSI_INFO_NODEV(fmt, arg ...)           SCSC_INFO(fmt, ## arg)

#undef SLSI_DBG1
#undef SLSI_DBG2
#undef SLSI_DBG3
#undef SLSI_DBG4
#define SLSI_DBG1(sdev, filter, fmt, arg ...)		SCSC_TAG_DBG1_SDEV(sdev, filter, fmt, ## arg)
#define SLSI_DBG2(sdev, filter, fmt, arg ...)		SCSC_TAG_DBG2_SDEV(sdev, filter, fmt, ## arg)
#define SLSI_DBG3(sdev, filter, fmt, arg ...)		SCSC_TAG_DBG3_SDEV(sdev, filter, fmt, ## arg)
#define SLSI_DBG4(sdev, filter, fmt, arg ...)		SCSC_TAG_DBG4_SDEV(sdev, filter, fmt, ## arg)

#undef SLSI_NET_DBG1
#undef SLSI_NET_DBG2
#undef SLSI_NET_DBG3
#undef SLSI_NET_DBG4
#define SLSI_NET_DBG1(ndev, filter, fmt, arg ...)	SCSC_TAG_DBG1_NDEV(ndev, filter, fmt, ## arg)
#define SLSI_NET_DBG2(ndev, filter, fmt, arg ...)	SCSC_TAG_DBG2_NDEV(ndev, filter, fmt, ## arg)
#define SLSI_NET_DBG3(ndev, filter, fmt, arg ...)	SCSC_TAG_DBG3_NDEV(ndev, filter, fmt, ## arg)
#define SLSI_NET_DBG4(ndev, filter, fmt, arg ...)	SCSC_TAG_DBG4_NDEV(ndev, filter, fmt, ## arg)

#undef SLSI_DBG1_NODEV
#undef SLSI_DBG2_NODEV
#undef SLSI_DBG3_NODEV
#undef SLSI_DBG4_NODEV
#define SLSI_DBG1_NODEV(filter, fmt, arg ...)		SCSC_TAG_DBG1(filter, fmt, ## arg)
#define SLSI_DBG2_NODEV(filter, fmt, arg ...)		SCSC_TAG_DBG2(filter, fmt, ## arg)
#define SLSI_DBG3_NODEV(filter, fmt, arg ...)		SCSC_TAG_DBG3(filter, fmt, ## arg)
#define SLSI_DBG4_NODEV(filter, fmt, arg ...)		SCSC_TAG_DBG4(filter, fmt, ## arg)

#endif /* CONFIG_SCSC_DEBUG_COMPATIBILITY */
#endif
