/*****************************************************************************
 *
 *   Copyright (c) 2016-2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/

#ifndef _SCSC_LOGRING_H_
#define _SCSC_LOGRING_H_
#include <linux/types.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/device.h>
#include <linux/sched/clock.h>

/* NOTE_CREATING_TAGS: when adding a tag here REMEMBER to add it also
 * where required, taking care to maintain the same ordering.
 * (Search 4 NOTE_CREATING_TAGS)
 *
 * You must update "int *scsc_droplevels[]" to match.
 */
enum scsc_logring_tags {
	FIRST_TAG,
	FIRST_BIN_TAG = FIRST_TAG,
	BINARY = FIRST_BIN_TAG,
	BIN_WIFI_CTRL_RX,
	BIN_WIFI_DATA_RX,
	BIN_WIFI_CTRL_TX,
	BIN_WIFI_DATA_TX,
	LAST_BIN_TAG = BIN_WIFI_DATA_TX,
	NO_TAG,
	WLBT = NO_TAG,
	WIFI_RX,
	WIFI_TX,
	BT_COMMON,
	BT_H4,
	BT_FW,
	BT_RX,
	BT_TX,
	CPKTBUFF,
	FW_LOAD,
	FW_PANIC,
	GDB_TRANS,
	MIF,
	CLK20,
	CLK20_TEST,
	FM,
	FM_TEST,
	MX_FILE,
	MX_FW,
	MX_SAMPLER,
	MXLOG_TRANS,
	MXMAN,
	MXMAN_TEST,
	MXMGT_TRANS,
	MX_MMAP,
	MX_PROC,
	PANIC_MON,
	PCIE_MIF,
	PLAT_MIF,
	KIC_COMMON,
	WLBTD,
	WLOG,
	LERNA,
	MX_CFG,
#ifdef CONFIG_SCSC_DEBUG_COMPATIBILITY
	SLSI_INIT_DEINIT,
	SLSI_NETDEV,
	SLSI_CFG80211,
	SLSI_MLME,
	SLSI_SUMMARY_FRAMES,
	SLSI_HYDRA,
	SLSI_TX,
	SLSI_RX,
	SLSI_UDI,
	SLSI_WIFI_FCQ,
	SLSI_HIP,
	SLSI_HIP_INIT_DEINIT,
	SLSI_HIP_FW_DL,
	SLSI_HIP_SDIO_OP,
	SLSI_HIP_PS,
	SLSI_HIP_TH,
	SLSI_HIP_FH,
	SLSI_HIP_SIG,
	SLSI_FUNC_TRACE,
	SLSI_TEST,
	SLSI_SRC_SINK,
	SLSI_FW_TEST,
	SLSI_RX_BA,
	SLSI_TDLS,
	SLSI_GSCAN,
	SLSI_MBULK,
	SLSI_FLOWC,
	SLSI_SMAPPER,
#endif
	TEST_ME,
	MAX_TAG = TEST_ME /* keep it last */
};


#define NODEV_LABEL	""
#define SCSC_SDEV_2_DEV(sdev) \
	(((sdev) && (sdev)->wiphy) ? &((sdev)->wiphy->dev) : NULL)
#define SCSC_NDEV_2_DEV(ndev) \
	((ndev) ? SCSC_SDEV_2_DEV(((struct netdev_vif *)netdev_priv(ndev))->sdev) : NULL)

#define SCSC_PREFIX	"wlbt: "	/* prepended to log statements */

#define SCSC_TAG_FMT(tag, fmt)		SCSC_PREFIX"[" # tag "]: %-5s: - %s: "fmt
#define SCSC_TAG_DBG_FMT(tag, fmt)	SCSC_PREFIX"[" # tag "]: %s: "fmt
#define SCSC_DEV_FMT(fmt)		SCSC_PREFIX"%-5s: - %s: "fmt
#define SCSC_DBG_FMT(fmt)		SCSC_PREFIX"%s: "fmt

int scsc_logring_enable(bool logging_enable);

#ifdef CONFIG_SCSC_PRINTK

int scsc_printk_tag(int force, int tag, const char *fmt, ...);
int scsc_printk_tag_dev(int force, int tag, struct device *dev, const char *fmt, ...);
int scsc_printk_tag_dev_lvl(int force, int tag, struct device *dev, int lvl, const char *fmt, ...);
int scsc_printk_tag_lvl(int tag, int lvl, const char *fmt, ...);
int scsc_printk_bin(int force, int tag, int dlev, const void *start, size_t len);

/**
 * This fields helps in trimming the behavior respect the kernel ring buffer:
 * - NO_FORCE_PRK: the tag-based filtering mechanism is obeyed.
 * - FORCE_PRK: the tag-based filtering is bypassed by this macro and message
 *   always get to the kernel ring buffer
 * - NO_ECHO_PRK: disable completely the printk redirect.
 */
#define NO_FORCE_PRK            0
#define FORCE_PRK               1
#define NO_ECHO_PRK             2

#define SCSC_PRINTK(args ...)            scsc_printk_tag(NO_FORCE_PRK, WLBT, \
							 args)

#define SCSC_PRINTK_TAG(tag, args ...)   scsc_printk_tag(NO_FORCE_PRK, (tag), \
							 args)
#define SCSC_PRINTK_BIN(start, len)      scsc_printk_bin(NO_FORCE_PRK, BINARY, \
							 -1, (start), (len))

#define SCSC_EMERG(fmt, args...)	scsc_printk_tag(NO_FORCE_PRK, WLBT, \
							KERN_EMERG SCSC_DBG_FMT(fmt), \
							 __func__, ## args)
#define SCSC_ALERT(fmt, args...)	scsc_printk_tag(NO_FORCE_PRK, WLBT, \
							KERN_ALERT SCSC_DBG_FMT(fmt), \
							 __func__, ## args)
#define SCSC_CRIT(fmt, args...)		scsc_printk_tag(NO_FORCE_PRK, WLBT, \
							KERN_CRIT SCSC_DBG_FMT(fmt), \
							 __func__, ## args)
#define SCSC_ERR(fmt, args...)		scsc_printk_tag(NO_FORCE_PRK, WLBT, \
							KERN_ERR SCSC_DBG_FMT(fmt), \
							 __func__, ## args)
#define SCSC_WARNING(fmt, args...)	scsc_printk_tag(NO_FORCE_PRK, WLBT, \
							KERN_WARNING SCSC_DBG_FMT(fmt), \
							 __func__, ## args)
#define SCSC_NOTICE(fmt, args...)	scsc_printk_tag(NO_FORCE_PRK, WLBT, \
							KERN_NOTICE SCSC_DBG_FMT(fmt), \
							 __func__, ## args)
#define SCSC_INFO(fmt, args...)		scsc_printk_tag(NO_FORCE_PRK, WLBT, \
							KERN_INFO SCSC_DBG_FMT(fmt), \
							 __func__, ## args)
#define SCSC_DEBUG(fmt, args...)	scsc_printk_tag(NO_FORCE_PRK, WLBT, \
							KERN_DEBUG SCSC_DBG_FMT(fmt), \
							 __func__, ## args)

#define SCSC_TAG_EMERG(tag, fmt, args...) scsc_printk_tag(NO_FORCE_PRK, (tag), \
							  KERN_EMERG SCSC_DBG_FMT(fmt), \
							  __func__, ## args)
#define SCSC_TAG_ALERT(tag, fmt, args...) scsc_printk_tag(NO_FORCE_PRK, (tag), \
							  KERN_ALERT SCSC_DBG_FMT(fmt), \
							  __func__, ## args)
#define SCSC_TAG_CRIT(tag, fmt, args ...) scsc_printk_tag(NO_FORCE_PRK, (tag), \
							  KERN_CRIT SCSC_DBG_FMT(fmt), \
							  __func__, ## args)
#define SCSC_TAG_ERR(tag, fmt, args...)  scsc_printk_tag(NO_FORCE_PRK, (tag), \
							 KERN_ERR SCSC_DBG_FMT(fmt), \
							 __func__, ## args)
#define SCSC_TAG_WARNING(tag, fmt, args...)  scsc_printk_tag(NO_FORCE_PRK, (tag), \
							     KERN_WARNING SCSC_DBG_FMT(fmt), \
							     __func__, ## args)
#define SCSC_TAG_NOTICE(tag, fmt, args...) scsc_printk_tag(NO_FORCE_PRK, (tag), \
							   KERN_NOTICE SCSC_DBG_FMT(fmt), \
							   __func__, ## args)
#define SCSC_TAG_INFO(tag, fmt, args...)   scsc_printk_tag(NO_FORCE_PRK, (tag), \
							   KERN_INFO SCSC_DBG_FMT(fmt), \
							   __func__, ## args)
#define SCSC_TAG_DEBUG(tag, fmt, args...)  scsc_printk_tag(NO_FORCE_PRK, (tag), \
							   KERN_DEBUG SCSC_DBG_FMT(fmt), \
							   __func__, ## args)

#define SCSC_TAG_ERR_SDEV(sdev, tag, fmt, args...) \
	scsc_printk_tag_dev(NO_FORCE_PRK, (tag), SCSC_SDEV_2_DEV((sdev)), \
			    KERN_ERR SCSC_DBG_FMT(fmt), \
			    __func__, ## args)
#define SCSC_TAG_WARNING_SDEV(sdev, tag, fmt, args...) \
	scsc_printk_tag_dev(NO_FORCE_PRK, (tag), SCSC_SDEV_2_DEV((sdev)), \
			    KERN_WARNING SCSC_DBG_FMT(fmt), \
			    __func__, ## args)
#define SCSC_TAG_INFO_SDEV(sdev, tag, fmt, args...) \
	scsc_printk_tag_dev(NO_FORCE_PRK, (tag), SCSC_SDEV_2_DEV((sdev)), \
			    KERN_INFO SCSC_DBG_FMT(fmt), \
			    __func__, ## args)
#define SCSC_TAG_DEBUG_SDEV(sdev, tag, fmt, args...) \
	scsc_printk_tag_dev(NO_FORCE_PRK, (tag), SCSC_SDEV_2_DEV((sdev)), \
			    KERN_DEBUG SCSC_DBG_FMT(fmt), \
			    __func__, ## args)

#define SCSC_TAG_ERR_NDEV(ndev, tag, fmt, args...) \
	scsc_printk_tag_dev(NO_FORCE_PRK, (tag), SCSC_NDEV_2_DEV((ndev)), \
			    KERN_ERR SCSC_DEV_FMT(fmt), \
			    ((ndev) ? netdev_name(ndev) : NODEV_LABEL), \
			    __func__, ## args)
#define SCSC_TAG_WARNING_NDEV(ndev, tag, fmt, args...) \
	scsc_printk_tag_dev(NO_FORCE_PRK, (tag), SCSC_NDEV_2_DEV((ndev)), \
			    KERN_WARNING SCSC_DEV_FMT(fmt), \
			    ((ndev) ? netdev_name(ndev) : NODEV_LABEL), \
			    __func__, ## args)
#define SCSC_TAG_INFO_NDEV(ndev, tag, fmt, args...) \
	scsc_printk_tag_dev(NO_FORCE_PRK, (tag), SCSC_NDEV_2_DEV((ndev)), \
			    KERN_INFO SCSC_DEV_FMT(fmt), \
			    ((ndev) ? netdev_name(ndev) : NODEV_LABEL), \
			    __func__, ## args)
#define SCSC_TAG_DEBUG_NDEV(ndev, tag, fmt, args...) \
	scsc_printk_tag_dev(NO_FORCE_PRK, (tag), SCSC_NDEV_2_DEV((ndev)), \
			    KERN_DEBUG SCSC_DEV_FMT(fmt), \
			    ((ndev) ? netdev_name(ndev) : NODEV_LABEL), \
			    __func__, ## args)

#define SCSC_TAG_ERR_DEV(tag, dev, fmt, args...) \
	scsc_printk_tag_dev(NO_FORCE_PRK, (tag), dev, \
			    KERN_ERR SCSC_DBG_FMT(fmt), \
			    __func__, ## args)

#define SCSC_TAG_WARNING_DEV(tag, dev, fmt, args...) \
	scsc_printk_tag_dev(NO_FORCE_PRK, (tag), dev, \
			    KERN_WARNING SCSC_DBG_FMT(fmt), \
			    __func__, ## args)

#define SCSC_TAG_INFO_DEV(tag, dev, fmt, args...) \
	scsc_printk_tag_dev(NO_FORCE_PRK, (tag), dev, \
			    KERN_INFO SCSC_DBG_FMT(fmt), \
			    __func__, ## args)

#define SCSC_TAG_DEBUG_DEV(tag, dev, fmt, args...) \
	scsc_printk_tag_dev(NO_FORCE_PRK, (tag), dev, \
			    KERN_DEBUG SCSC_DBG_FMT(fmt), \
			    __func__, ## args)

#define SCSC_ERR_SDEV(sdev, fmt, args...) \
	SCSC_TAG_ERR_SDEV(sdev, WLBT, fmt, ## args)
#define SCSC_WARNING_SDEV(sdev, fmt, args...) \
	SCSC_TAG_WARNING_SDEV(sdev, WLBT, fmt, ## args)
#define SCSC_INFO_SDEV(sdev, fmt, args...) \
	SCSC_TAG_INFO_SDEV(sdev, WLBT, fmt, ## args)

#define SCSC_ERR_NDEV(ndev, fmt, args...) \
	SCSC_TAG_ERR_NDEV(ndev, WLBT, fmt, ## args)
#define SCSC_WARNING_NDEV(ndev, fmt, args...) \
	SCSC_TAG_WARNING_NDEV(ndev, WLBT, fmt, ## args)
#define SCSC_INFO_NDEV(ndev, fmt, args...) \
	SCSC_TAG_INFO_NDEV(ndev, WLBT, fmt, ## args)


#define SCSC_BIN_EMERG(start, len)      scsc_printk_bin(NO_FORCE_PRK, BINARY, 0, \
							(start), (len))
#define SCSC_BIN_ALERT(start, len)      scsc_printk_bin(NO_FORCE_PRK, BINARY, 1, \
							(start), (len))
#define SCSC_BIN_CRIT(start, len)       scsc_printk_bin(NO_FORCE_PRK, BINARY, 2, \
							(start), (len))
#define SCSC_BIN_ERR(start, len)        scsc_printk_bin(NO_FORCE_PRK, BINARY, 3, \
							(start), (len))
#define SCSC_BIN_WARNING(start, len)    scsc_printk_bin(NO_FORCE_PRK, BINARY, 4, \
							(start), (len))
#define SCSC_BIN_NOTICE(start, len)     scsc_printk_bin(NO_FORCE_PRK, BINARY, 5, \
							(start), (len))
#define SCSC_BIN_INFO(start, len)       scsc_printk_bin(NO_FORCE_PRK, BINARY, 6, \
							(start), (len))
#define SCSC_BIN_DEBUG(start, len)      scsc_printk_bin(NO_FORCE_PRK, BINARY, 7, \
							(start), (len))

#define SCSC_BIN_TAG_EMERG(tag, start, len)      scsc_printk_bin(NO_FORCE_PRK, (tag), 0, \
								 (start), (len))
#define SCSC_BIN_TAG_ALERT(tag, start, len)      scsc_printk_bin(NO_FORCE_PRK, (tag), 1, \
								 (start), (len))
#define SCSC_BIN_TAG_CRIT(tag, start, len)       scsc_printk_bin(NO_FORCE_PRK, (tag), 2, \
								 (start), (len))
#define SCSC_BIN_TAG_ERR(tag, start, len)        scsc_printk_bin(NO_FORCE_PRK, (tag), 3, \
								 (start), (len))
#define SCSC_BIN_TAG_WARNING(tag, start, len)    scsc_printk_bin(NO_FORCE_PRK, (tag), 4, \
								 (start), (len))
#define SCSC_BIN_TAG_NOTICE(tag, start, len)     scsc_printk_bin(NO_FORCE_PRK, (tag), 5, \
								 (start), (len))
#define SCSC_BIN_TAG_INFO(tag, start, len)       scsc_printk_bin(NO_FORCE_PRK, (tag), 6, \
								 (start), (len))
#define SCSC_BIN_TAG_DEBUG(tag, start, len)      scsc_printk_bin(NO_FORCE_PRK, (tag), 7, \
								 (start), (len))


/*
 * These macros forces a redundant copy of their output to kernel log buffer and
 * console through standard kernel facilities, NO matter how the tag-based
 * filtering is configured and NO matter what the value in
 * scsc_redirect_to_printk_droplvl module param.
 */
#define SCSC_PRINTK_FF(args ...)		scsc_printk_tag(FORCE_PRK, WLBT, args)
#define SCSC_PRINTK_TAG_FF(tag, args ...)	scsc_printk_tag(FORCE_PRK, (tag), args)
#define SCSC_PRINTK_BIN_FF(start, len)		scsc_printk_bin(FORCE_PRK, -1, \
								(start), (len))

#define SCSC_EMERG_FF(args ...)          scsc_printk_tag(FORCE_PRK, WLBT, \
							 KERN_EMERG SCSC_PREFIX args)
#define SCSC_ALERT_FF(args ...)          scsc_printk_tag(FORCE_PRK, WLBT, \
							 KERN_ALERT SCSC_PREFIX args)
#define SCSC_CRIT_FF(args ...)           scsc_printk_tag(FORCE_PRK, WLBT, \
							 KERN_CRIT SCSC_PREFIX args)
#define SCSC_ERR_FF(args ...)            scsc_printk_tag(FORCE_PRK, WLBT, \
							 KERN_ERR SCSC_PREFIX args)
#define SCSC_WARNING_FF(args ...)        scsc_printk_tag(FORCE_PRK, WLBT, \
							 KERN_WARNING SCSC_PREFIX args)
#define SCSC_NOTICE_FF(args ...)         scsc_printk_tag(FORCE_PRK, WLBT, \
							 KERN_NOTICE SCSC_PREFIX args)
#define SCSC_INFO_FF(args ...)           scsc_printk_tag(FORCE_PRK, WLBT, \
							 KERN_INFO SCSC_PREFIX args)
#define SCSC_DEBUG_FF(args ...)          scsc_printk_tag(FORCE_PRK, WLBT, \
							 KERN_DEBUG SCSC_PREFIX args)

#define SCSC_TAG_EMERG_FF(tag, args ...) scsc_printk_tag(FORCE_PRK, (tag), \
							 KERN_EMERG SCSC_PREFIX args)
#define SCSC_TAG_ALERT_FF(tag, args ...) scsc_printk_tag(FORCE_PRK, (tag), \
							 KERN_ALERT SCSC_PREFIX args)
#define SCSC_TAG_CRIT_FF(tag, args ...)  scsc_printk_tag(FORCE_PRK, (tag), \
							 KERN_CRIT SCSC_PREFIX args)
#define SCSC_TAG_ERR_FF(tag, args ...)   scsc_printk_tag(FORCE_PRK, (tag), \
							 KERN_ERR SCSC_PREFIX args)
#define SCSC_TAG_WARNING_FF(tag, args ...) scsc_printk_tag(FORCE_PRK, (tag), \
							   KERN_WARNING SCSC_PREFIX args)
#define SCSC_TAG_NOTICE_FF(tag, args ...) scsc_printk_tag(FORCE_PRK, (tag), \
							  KERN_NOTICE SCSC_PREFIX args)
#define SCSC_TAG_INFO_FF(tag, args ...)  scsc_printk_tag(FORCE_PRK, (tag), \
							 KERN_INFO SCSC_PREFIX args)
#define SCSC_TAG_DEBUG_FF(tag, args ...) scsc_printk_tag(FORCE_PRK, (tag), \
							 KERN_DEBUG SCSC_PREFIX args)

#define SCSC_BIN_EMERG_FF(start, len)   scsc_printk_bin(FORCE_PRK, 0, \
							(start), (len))
#define SCSC_BIN_ALERT_FF(start, len)   scsc_printk_bin(FORCE_PRK, 1, \
							(start), (len))
#define SCSC_BIN_CRIT_FF(start, len)    scsc_printk_bin(FORCE_PRK, 2, \
							(start), (len))
#define SCSC_BIN_ERR_FF(start, len)     scsc_printk_bin(FORCE_PRK, 3, \
							(start), (len))
#define SCSC_BIN_WARNING_FF(start, len) scsc_printk_bin(FORCE_PRK, 4, \
							(start), (len))
#define SCSC_BIN_NOTICE_FF(start, len)  scsc_printk_bin(FORCE_PRK, 5, \
							(start), (len))
#define SCSC_BIN_INFO_FF(start, len)    scsc_printk_bin(FORCE_PRK, 6, \
							(start), (len))
#define SCSC_BIN_DEBUG_FF(start, len)   scsc_printk_bin(FORCE_PRK, 7, \
							(start), (len))

#define SCSC_TAG_LVL(tag, lvl, fmt, args...)	\
	scsc_printk_tag_lvl((tag), (lvl), fmt, ## args)

#define SCSC_TAG_DEV_LVL(tag, lvl, dev, fmt, args...)	\
	scsc_printk_tag_dev_lvl(NO_FORCE_PRK, (tag), (dev), (lvl), fmt, ## args)


#define SCSC_TAG_DBG1_SDEV(sdev, tag, fmt, args...) \
	SCSC_TAG_DEV_LVL((tag), 7, SCSC_SDEV_2_DEV((sdev)), \
			 SCSC_DBG_FMT(fmt), __func__, ## args)

#define SCSC_TAG_DBG2_SDEV(sdev, tag, fmt, args...) \
	SCSC_TAG_DEV_LVL((tag), 8, SCSC_SDEV_2_DEV((sdev)), \
			 SCSC_DBG_FMT(fmt), __func__, ## args)

#define SCSC_TAG_DBG3_SDEV(sdev, tag, fmt, args...) \
	SCSC_TAG_DEV_LVL((tag), 9, SCSC_SDEV_2_DEV((sdev)), \
			 SCSC_DBG_FMT(fmt), __func__, ## args)

#define SCSC_TAG_DBG4_SDEV(sdev, tag, fmt, args...) \
	SCSC_TAG_DEV_LVL((tag), 10, SCSC_SDEV_2_DEV((sdev)), \
			 SCSC_DBG_FMT(fmt), __func__, ## args)

#define SCSC_TAG_DBG1_NDEV(ndev, tag, fmt, args...) \
	SCSC_TAG_DEV_LVL((tag), 7, SCSC_NDEV_2_DEV((ndev)), SCSC_DEV_FMT(fmt), \
			 ((ndev) ? netdev_name(ndev) : NODEV_LABEL), \
			 __func__, ## args)

#define SCSC_TAG_DBG2_NDEV(ndev, tag, fmt, args...) \
	SCSC_TAG_DEV_LVL((tag), 8, SCSC_NDEV_2_DEV((ndev)), SCSC_DEV_FMT(fmt), \
			 ((ndev) ? netdev_name(ndev) : NODEV_LABEL), \
			 __func__, ## args)

#define SCSC_TAG_DBG3_NDEV(ndev, tag, fmt, args...) \
	SCSC_TAG_DEV_LVL((tag), 9, SCSC_NDEV_2_DEV((ndev)), SCSC_DEV_FMT(fmt), \
			 ((ndev) ? netdev_name(ndev) : NODEV_LABEL), \
			 __func__, ## args)

#define SCSC_TAG_DBG4_NDEV(ndev, tag, fmt, args...) \
	SCSC_TAG_DEV_LVL((tag), 10, SCSC_NDEV_2_DEV((ndev)), SCSC_DEV_FMT(fmt), \
			 ((ndev) ? netdev_name(ndev) : NODEV_LABEL), \
			 __func__, ## args)

#define SCSC_TAG_DBG1(tag, fmt, args ...) \
	SCSC_TAG_LVL((tag), 7, fmt, ## args)

#define SCSC_TAG_DBG2(tag, fmt, args ...) \
	SCSC_TAG_LVL((tag), 8, fmt, ## args)

#define SCSC_TAG_DBG3(tag, fmt, args ...) \
	SCSC_TAG_LVL((tag), 9, fmt, ## args)

#define SCSC_TAG_DBG4(tag, fmt, args ...) \
	SCSC_TAG_LVL((tag), 10, fmt, ## args)

#else /* CONFIG_SCSC_PRINTK */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#define SCSC_TAG_LVL(tag, lvl, fmt, args...)	\
	do {\
		if ((lvl) < 7)\
			dev_printk_emit((lvl), NULL, fmt, ## args);\
	} while (0)
#else
#define SCSC_TAG_DEV_LVL(tag, lvl, dev, fmt, args...) \
	do {\
		if ((lvl) < 7)\
			printk_emit((lvl), (dev), fmt, ## args);\
	} while (0)
#endif
#define SCSC_PRINTK(fmt, args ...)               printk(SCSC_PREFIX fmt, ## args)
#define SCSC_PRINTK_TAG(tag, fmt, args ...)      printk(SCSC_PREFIX "[" # tag "] "fmt, ## args)
#define SCSC_PRINTK_BIN(start, len)              print_hex_dump(KERN_INFO, \
							       SCSC_PREFIX"[BINARY]->|", \
							       DUMP_PREFIX_ADDRESS, \
							       16, 4, start, \
							       len, true)

#define SCSC_PRINTK_FF(fmt, args ...)            printk(SCSC_PREFIX fmt, ## args)
#define SCSC_PRINTK_TAG_FF(tag, fmt, args ...)   printk(SCSC_PREFIX"[" # tag "] "fmt, ## args)
#define SCSC_PRINTK_BIN_FF(start, len)           print_hex_dump(KERN_INFO, \
							       SCSC_PREFIX"[BINARY]->|", \
							       DUMP_PREFIX_ADDRESS, \
							       16, 4, start, \
							       len, true)

#define SCSC_EMERG(fmt, args...)        pr_emerg(SCSC_DBG_FMT(fmt), __func__, ## args)
#define SCSC_ALERT(fmt, args...)        pr_alert(SCSC_DBG_FMT(fmt), __func__, ## args)
#define SCSC_CRIT(fmt, args...)         pr_crit(SCSC_DBG_FMT(fmt), __func__, ## args)
#define SCSC_ERR(fmt, args...)          pr_err(SCSC_DBG_FMT(fmt), __func__, ## args)
#define SCSC_WARNING(fmt, args...)      pr_warn(SCSC_DBG_FMT(fmt), __func__, ## args)
#define SCSC_NOTICE(fmt, args...)	pr_notice(SCSC_DBG_FMT(fmt), __func__, ## args)
#define SCSC_INFO(fmt, args...)		pr_info(SCSC_DBG_FMT(fmt), __func__, ## args)
#define SCSC_DEBUG(args...)		do {} while (0)

/* Reverting to pr_* keeping the [tag] */
#define SCSC_TAG_EMERG(tag, fmt, args...)       \
	pr_emerg(SCSC_TAG_DBG_FMT(tag, fmt), __func__, ## args)
#define SCSC_TAG_ALERT(tag, fmt, args...)       \
	pr_alert(SCSC_TAG_DBG_FMT(tag, fmt), __func__, ## args)
#define SCSC_TAG_CRIT(tag, fmt, args...)        \
	pr_crit(SCSC_TAG_DBG_FMT(tag, fmt), __func__, ## args)
#define SCSC_TAG_ERR(tag, fmt, args...)         \
	pr_err(SCSC_TAG_DBG_FMT(tag, fmt), __func__, ## args)
#define SCSC_TAG_WARNING(tag, fmt, args...)     \
	pr_warn(SCSC_TAG_DBG_FMT(tag, fmt), __func__, ## args)
#define SCSC_TAG_NOTICE(tag, fmt, args...)      \
	pr_notice(SCSC_TAG_DBG_FMT(tag, fmt), __func__, ## args)
#define SCSC_TAG_INFO(tag, fmt, args...)	 \
	pr_info(SCSC_TAG_DBG_FMT(tag, fmt), __func__, ## args)
#define SCSC_TAG_DEBUG(tag, fmt, args...)        do {} while (0)


#define SCSC_BIN_EMERG(start, len)      print_hex_dump(KERN_EMERG, \
						       SCSC_PREFIX"[BINARY]->|", \
						       DUMP_PREFIX_ADDRESS, \
						       16, 4, start, \
						       len, true)

#define SCSC_BIN_ALERT(start, len)      print_hex_dump(KERN_ALERT, \
						       SCSC_PREFIX"[BINARY]->|", \
						       DUMP_PREFIX_ADDRESS, \
						       16, 4, start, \
						       len, true)

#define SCSC_BIN_CRIT(start, len)       print_hex_dump(KERN_CRIT, \
						       SCSC_PREFIX"[BINARY]->|", \
						       DUMP_PREFIX_ADDRESS, \
						       16, 4, start, \
						       len, true)

#define SCSC_BIN_ERR(start, len)        print_hex_dump(KERN_ERR, \
						       SCSC_PREFIX"[BINARY]->|", \
						       DUMP_PREFIX_ADDRESS, \
						       16, 4, start, \
						       len, true)

#define SCSC_BIN_WARNING(start, len)    print_hex_dump(KERN_WARNING, \
						       SCSC_PREFIX"[BINARY]->|", \
						       DUMP_PREFIX_ADDRESS, \
						       16, 4, start, \
						       len, true)

#define SCSC_BIN_NOTICE(start, len)     print_hex_dump(KERN_NOTICE, \
						       SCSC_PREFIX"[BINARY]->|", \
						       DUMP_PREFIX_ADDRESS, \
						       16, 4, start, \
						       len, true)

#define SCSC_BIN_INFO(start, len)       print_hex_dump(KERN_INFO, \
						       SCSC_PREFIX"[BINARY]->|", \
						       DUMP_PREFIX_ADDRESS, \
						       16, 4, start, \
						       len, true)

#define SCSC_BIN_DEBUG(start, len)       do {} while (0)


#define SCSC_BIN_TAG_EMERG(tag, start, len)      print_hex_dump(KERN_EMERG, \
								SCSC_PREFIX"[" # tag "]->|", \
								DUMP_PREFIX_ADDRESS, \
								16, 4, start, \
								len, true)

#define SCSC_BIN_TAG_ALERT(tag, start, len)      print_hex_dump(KERN_ALERT, \
								SCSC_PREFIX"[" # tag "]->|", \
								DUMP_PREFIX_ADDRESS, \
								16, 4, start, \
								len, true)

#define SCSC_BIN_TAG_CRIT(tag, start, len)       print_hex_dump(KERN_CRIT, \
								SCSC_PREFIX"[" # tag "]->|", \
								DUMP_PREFIX_ADDRESS, \
								16, 4, start, \
								len, true)

#define SCSC_BIN_TAG_ERR(tag, start, len)        print_hex_dump(KERN_ERR, \
								SCSC_PREFIX"[" # tag "]->|", \
								DUMP_PREFIX_ADDRESS, \
								16, 4, start, \
								len, true)

#define SCSC_BIN_TAG_WARNING(tag, start, len)    print_hex_dump(KERN_WARNING, \
								SCSC_PREFIX"[" # tag "]->|", \
								DUMP_PREFIX_ADDRESS, \
								16, 4, start, \
								len, true)

#define SCSC_BIN_TAG_NOTICE(tag, start, len)     print_hex_dump(KERN_NOTICE, \
								SCSC_PREFIX"[" # tag "]->|", \
								DUMP_PREFIX_ADDRESS, \
								16, 4, start, \
								len, true)

#define SCSC_BIN_TAG_INFO(tag, start, len)       print_hex_dump(KERN_INFO, \
								SCSC_PREFIX"[" # tag "]->|", \
								DUMP_PREFIX_NONE, \
								16, 1, start, \
								len, false)

#define SCSC_BIN_TAG_DEBUG(tag, start, len)       do {} while (0)


#define SCSC_EMERG_FF(args ...)          pr_emerg(SCSC_PREFIX args)
#define SCSC_ALERT_FF(args ...)          pr_alert(SCSC_PREFIX args)
#define SCSC_CRIT_FF(args ...)           pr_crit(SCSC_PREFIX args)
#define SCSC_ERR_FF(args ...)            pr_err(SCSC_PREFIX args)
#define SCSC_WARNING_FF(args ...)        pr_warn(SCSC_PREFIX args)
#define SCSC_NOTICE_FF(args ...)         pr_notice(SCSC_PREFIX args)
#define SCSC_INFO_FF(args ...)           pr_info(SCSC_PREFIX args)
#define SCSC_DEBUG_FF(args ...)          do {} while (0)


#define SCSC_TAG_EMERG_FF(tag, fmt, args ...)	pr_emerg(SCSC_TAG_DBG_FMT(tag, fmt), \
							__func__, ## args)
#define SCSC_TAG_ALERT_FF(tag, fmt, args ...)   pr_alert(SCSC_TAG_DBG_FMT(tag, fmt), \
							__func__, ## args)
#define SCSC_TAG_CRIT_FF(tag, fmt, args ...)    pr_crit(SCSC_TAG_DBG_FMT(tag, fmt), \
							__func__, ## args)
#define SCSC_TAG_ERR_FF(tag, fmt, args ...)	pr_err(SCSC_TAG_DBG_FMT(tag, fmt), \
							__func__, ## args)
#define SCSC_TAG_WARNING_FF(tag, fmt, args ...) pr_warn(SCSC_TAG_DBG_FMT(tag, fmt), \
							__func__, ## args)
#define SCSC_TAG_NOTICE_FF(tag, fmt, args ...)	pr_notice(SCSC_TAG_DBG_FMT(tag, fmt), \
							__func__, ## args)
#define SCSC_TAG_INFO_FF(tag, fmt, args ...)    pr_info(SCSC_TAG_DBG_FMT(tag, fmt), \
							__func__, ## args)
#define SCSC_TAG_DEBUG_FF(tag, fmt, args ...)   do {} while (0)

#define SCSC_BIN_EMERG_FF(start, len)           print_hex_dump(KERN_EMERG, \
							       SCSC_PREFIX"[BINARY]->|", \
							       DUMP_PREFIX_ADDRESS, \
							       16, 4, start, \
							       len, true)

#define SCSC_BIN_ALERT_FF(start, len)           print_hex_dump(KERN_ALERT, \
							       SCSC_PREFIX"[BINARY]->|", \
							       DUMP_PREFIX_ADDRESS, \
							       16, 4, start, \
							       len, true)

#define SCSC_BIN_CRIT_FF(start, len)            print_hex_dump(KERN_CRIT, \
							       SCSC_PREFIX"[BINARY]->|", \
							       DUMP_PREFIX_ADDRESS, \
							       16, 4, start, \
							       len, true)

#define SCSC_BIN_ERR_FF(start, len)             print_hex_dump(KERN_ERR, \
							       SCSC_PREFIX"[BINARY]->|", \
							       DUMP_PREFIX_ADDRESS, \
							       16, 4, start, \
							       len, true)

#define SCSC_BIN_WARNING_FF(start, len)         print_hex_dump(KERN_WARNING, \
							       SCSC_PREFIX"[BINARY]->|", \
							       DUMP_PREFIX_ADDRESS, \
							       16, 4, start, \
							       len, true)

#define SCSC_BIN_NOTICE_FF(start, len)          print_hex_dump(KERN_NOTICE, \
							       SCSC_PREFIX"[BINARY]->|", \
							       DUMP_PREFIX_ADDRESS, \
							       16, 4, start, \
							       len, true)

#define SCSC_BIN_INFO_FF(start, len)            print_hex_dump(KERN_INFO, \
							       SCSC_PREFIX"[BINARY]->|", \
							       DUMP_PREFIX_ADDRESS, \
							       16, 4, start, \
							       len, true)

#define SCSC_BIN_DEBUG_FF(start, len)           do {} while (0)


#define SCSC_TAG_ERR_SDEV(sdev, tag, fmt, args...) \
	dev_err(SCSC_SDEV_2_DEV((sdev)), SCSC_TAG_DBG_FMT(tag, fmt), \
		__func__, ## args)

#define SCSC_TAG_WARNING_SDEV(sdev, tag, fmt, args...) \
	dev_warn(SCSC_SDEV_2_DEV((sdev)), SCSC_TAG_DBG_FMT(tag, fmt), \
		 __func__, ## args)

#define SCSC_TAG_INFO_SDEV(sdev, tag, fmt, args...) \
	dev_info(SCSC_SDEV_2_DEV((sdev)), SCSC_TAG_DBG_FMT(tag, fmt), \
		 __func__, ## args)

#define SCSC_TAG_DEBUG_SDEV(sdev, tag, fmt, args...)	do {} while (0)


#define SCSC_TAG_ERR_NDEV(ndev, tag, fmt, args...) \
	dev_err(SCSC_NDEV_2_DEV((ndev)), SCSC_TAG_FMT(tag, fmt), \
		((ndev) ? netdev_name(ndev) : NODEV_LABEL), __func__, ## args)
#define SCSC_TAG_WARNING_NDEV(ndev, tag, fmt, args...) \
	dev_warn(SCSC_NDEV_2_DEV((ndev)), SCSC_TAG_FMT(tag, fmt), \
		    ((ndev) ? netdev_name(ndev) : NODEV_LABEL), __func__, ## args)
#define SCSC_TAG_INFO_NDEV(ndev, tag, fmt, args...) \
	dev_info(SCSC_NDEV_2_DEV((ndev)), SCSC_TAG_FMT(tag, fmt), \
		 ((ndev) ? netdev_name(ndev) : NODEV_LABEL), __func__, ## args)

#define SCSC_TAG_DEBUG_NDEV(ndev, tag, fmt, args...)	do {} while (0)

#define SCSC_TAG_ERR_DEV(tag, dev, fmt, args...) \
	dev_err(dev, SCSC_TAG_DBG_FMT(tag, fmt), \
		__func__, ## args)

#define SCSC_TAG_WARNING_DEV(tag, dev, fmt, args...) \
	dev_warn(dev, SCSC_TAG_DBG_FMT(tag, fmt), \
		 __func__, ## args)

#define SCSC_TAG_INFO_DEV(tag, dev, fmt, args...) \
	dev_info(dev, SCSC_TAG_DBG_FMT(tag, fmt), \
		 __func__, ## args)

#define SCSC_TAG_DEBUG_DEV(tag, dev, fmt, args...)	do {} while (0)

#define SCSC_ERR_SDEV(sdev, fmt, args...) \
	SCSC_TAG_ERR_SDEV(sdev, WLBT, fmt, ## args)
#define SCSC_WARNING_SDEV(sdev, fmt, args...) \
	SCSC_TAG_WARNING_SDEV(sdev, WLBT, fmt, ## args)
#define SCSC_INFO_SDEV(sdev, fmt, args...) \
	SCSC_TAG_INFO_SDEV(sdev, WLBT, fmt, ## args)

#define SCSC_ERR_NDEV(ndev, fmt, args...) \
	SCSC_TAG_ERR_NDEV(ndev, WLBT, fmt, ## args)
#define SCSC_WARNING_NDEV(ndev, fmt, args...) \
	SCSC_TAG_WARNING_NDEV(ndev, WLBT, fmt, ## args)
#define SCSC_INFO_NDEV(ndev, fmt, args...) \
	SCSC_TAG_INFO_NDEV(ndev, WLBT, fmt, ## args)


#define SCSC_TAG_DBG1_SDEV(sdev, tag, fmt, args...)	do {} while (0)
#define SCSC_TAG_DBG2_SDEV(sdev, tag, fmt, args...)	do {} while (0)
#define SCSC_TAG_DBG3_SDEV(sdev, tag, fmt, args...)	do {} while (0)
#define SCSC_TAG_DBG4_SDEV(sdev, tag, fmt, args...)	do {} while (0)

#define SCSC_TAG_DBG1_NDEV(ndev, tag, fmt, args...)	do {} while (0)
#define SCSC_TAG_DBG2_NDEV(ndev, tag, fmt, args...)	do {} while (0)
#define SCSC_TAG_DBG3_NDEV(ndev, tag, fmt, args...)	do {} while (0)
#define SCSC_TAG_DBG4_NDEV(ndev, tag, fmt, args...)	do {} while (0)

#define SCSC_TAG_DBG1(tag, fmt, args ...)		do {} while (0)
#define SCSC_TAG_DBG2(tag, fmt, args ...)		do {} while (0)
#define SCSC_TAG_DBG3(tag, fmt, args ...)		do {} while (0)
#define SCSC_TAG_DBG4(tag, fmt, args ...)		do {} while (0)

#endif

/* callbacks to mxman */
struct scsc_logring_mx_cb {
	int (*scsc_logring_register_observer)(struct scsc_logring_mx_cb *mx_cb, char *name);
	int (*scsc_logring_unregister_observer)(struct scsc_logring_mx_cb *mx_cb, char *name);
};

int scsc_logring_register_mx_cb(struct scsc_logring_mx_cb *mx_cb);
int scsc_logring_unregister_mx_cb(struct scsc_logring_mx_cb *mx_cb);

#endif /* _SCSC_LOGRING_H_ */
