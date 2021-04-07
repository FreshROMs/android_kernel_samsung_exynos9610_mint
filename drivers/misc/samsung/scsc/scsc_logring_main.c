/********************************************************************************
 *
 *   Copyright (c) 2016 - 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 ********************************************************************************/
#include "scsc_logring_main.h"
#include "scsc_logring_ring.h"
#include "scsc_logring_debugfs.h"
#include <scsc/scsc_logring.h>
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
#include <scsc/scsc_log_collector.h>
#endif
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
#include "soc/samsung/memlogger.h"
#endif

/* Global module parameters */
static int              cached_enable;
static int              enable = DEFAULT_ENABLE_LOGRING;
static bool             initialized;
#ifndef CONFIG_SCSC_STATIC_RING_SIZE
static int              ringsize = DEFAULT_RING_BUFFER_SZ;
#else
static int		ringsize = CONFIG_SCSC_STATIC_RING_SIZE;
#endif
static int              prepend_header = DEFAULT_ENABLE_HEADER;
static int              default_dbglevel = DEFAULT_DBGLEVEL;
static int              scsc_droplevel_wlbt = DEFAULT_DROPLEVEL;
static int              scsc_droplevel_all = DEFAULT_ALL_DISABLED;
static int              scsc_droplevel_atomic = DEFAULT_DROPLEVEL;
static int              scsc_redirect_to_printk_droplvl = DEFAULT_REDIRECT_DROPLVL;
static int              scsc_reset_all_droplevels_to;

static struct scsc_ring_buffer *the_ringbuf;

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
static int logring_collect(struct scsc_log_collector_client *collect_client, size_t size);

static struct scsc_log_collector_client logring_collect_client = {
	.name = "Logring",
	.type = SCSC_LOG_CHUNK_LOGRING,
	.collect_init = NULL,
	.collect = logring_collect,
	.collect_end = NULL,
	.prv = NULL,
};
#endif

/* Module init and ring buffer allocation */
int __init samlog_init(void)
{
	struct scsc_ring_buffer *rb = NULL;

	pr_debug("wlbt: Samlog Init\n");
	if (!enable) {
		pr_info("wlbt: Samlog: module disabled...NOT starting.\n");
		return 0;
	}
	if (the_ringbuf != NULL) {
		pr_info("wlbt: Samlog: Ring:%s already initialized...skipping.\n",
			the_ringbuf->name);
		return 0;
	}
	/* Check for power of two compliance with std Kernel func */
	if (!is_power_of_2(ringsize)) {
		ringsize = DEFAULT_RING_BUFFER_SZ;
		pr_info("wlbt: Samlog: scsc_logring.ringsize MUST be power-of-two. Using default: %d\n",
			ringsize);
	}
	rb = alloc_ring_buffer(ringsize, BASE_SPARE_SZ, DEBUGFS_RING0_ROOT);

	if (!rb)
		goto tfail;
	rb->private = samlog_debugfs_init(rb->name, rb);
	if (!rb->private)
		pr_info("wlbt: Samlog: Cannot Initialize DebugFS.\n");
#ifndef CONFIG_SCSC_STATIC_RING_SIZE
	pr_info("wlbt: scsc_logring:: Allocated ring buffer of size %zd bytes.\n",
		rb->bsz);
#else
	pr_info("wlbt: scsc_logring: Allocated STATIC ring buffer of size %zd bytes.\n",
		rb->bsz);
#endif
	the_ringbuf = rb;
	initialized = true;
	pr_info("wlbt: Samlog Loaded.\n");
	scsc_printk_tag(FORCE_PRK, NO_TAG, "wlbt: Samlog Started.\n");
	scsc_printk_tag(NO_ECHO_PRK, NO_TAG,
	                "wlbt: Allocated ring buffer of size %zd bytes at %p - %p\n",
	                rb->bsz, virt_to_phys((const volatile void *)rb->buf),
	                virt_to_phys((const volatile void *)(rb->buf + rb->bsz)));
	scsc_printk_tag(NO_ECHO_PRK, NO_TAG,
			"wlbt: Using THROWAWAY DYNAMIC per-reader buffer.\n");

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	scsc_log_collector_register_client(&logring_collect_client);
#endif
	return 0;

tfail:
	pr_err("wlbt: Samlog Initialization Failed. LogRing disabled.\n");
	return -ENODEV;
}

void __exit samlog_exit(void)
{
	if (!the_ringbuf) {
		pr_err("wlbt: Cannot UNLOAD ringbuf\n");
		return;
	}
	if (the_ringbuf && the_ringbuf->private)
		samlog_debugfs_exit(&the_ringbuf->private);
	initialized = false;
	free_ring_buffer(the_ringbuf);
	the_ringbuf = NULL;
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	scsc_log_collector_unregister_client(&logring_collect_client);
#endif
	pr_info("Samlog Unloaded\n");
}

module_init(samlog_init);
module_exit(samlog_exit);

module_param(enable, int, S_IRUGO | S_IWUSR);
SCSC_MODPARAM_DESC(enable, "Enable/Disable scsc_logring as a whole.",
		   "load-time", DEFAULT_ENABLE_LOGRING);

#ifndef CONFIG_SCSC_STATIC_RING_SIZE
module_param(ringsize, int, S_IRUGO);
SCSC_MODPARAM_DESC(ringsize,
		   "Ring buffer size. Available ONLY if ring is NOT statically allocated.",
		   "run-time", DEFAULT_RING_BUFFER_SZ);
#endif

module_param(prepend_header, int, S_IRUGO | S_IWUSR);
SCSC_MODPARAM_DESC(prepend_header, "Enable/disable header prepending. ",
		   "run-time", DEFAULT_ENABLE_HEADER);

module_param(default_dbglevel, int, S_IRUGO | S_IWUSR);
SCSC_MODPARAM_DESC(default_dbglevel,
		   "The default debug level assigned to messages when NOT explicitly specified.",
		   "run-time", DEFAULT_DBGLEVEL);

module_param(scsc_droplevel_wlbt, int, S_IRUGO | S_IWUSR);
SCSC_MODPARAM_DESC(scsc_droplevel_wlbt,
		   "Droplevels for the 'no_tag/wlbt' tag family.", "run-time",
		   DEFAULT_DROP_ALL);

module_param(scsc_droplevel_all, int, S_IRUGO | S_IWUSR);
SCSC_MODPARAM_DESC(scsc_droplevel_all,
		   "This droplevel overrides any other, if set to a value >= 0",
		   "run-time", DEFAULT_ALL_DISABLED);

module_param(scsc_redirect_to_printk_droplvl, int, S_IRUGO | S_IWUSR);
SCSC_MODPARAM_DESC(scsc_redirect_to_printk_droplvl,
		   "Echoing messages up to the specified loglevel also to kernel standard ring buffer.",
		   "run-time", DEFAULT_REDIRECT_DROPLVL);

module_param(scsc_droplevel_atomic, int, S_IRUGO | S_IWUSR);
SCSC_MODPARAM_DESC(scsc_droplevel_atomic,
		   "This droplevel is applied to logmsg emitted in atomic context.",
		   "run-time", DEFAULT_KEEP_ALL);

/**
 * This macro code has been freely 'inspired' (read copied) from the
 * slsi_ original/old debug.c implementaion: it takes care to register
 * a new TAG with the debug subsystem including a module param to
 * dynamically configure the droplevel for the specified tag at runtime.
 *
 * ALL debug is ON by default at FULL DEBUG LEVEL.
 * There are THREE intended exceptions to this that instead stick to
 * level 7: PLAT_MIF MBULK and MXLOG_TRANS tags.
 *
 * NOTE_CREATING_TAGS: when adding a tag here REMEMBER to add it also
 * where required, taking care to maintain the same ordering.
 * (Search 4 NOTE_CREATING_TAGS)
 */

/**
 * This macros define module parameters used to configure per-subsystem
 * filtering, and assign a default DROPLEVEL.
 *
 * NOTE THAT the default DROPLEVEL indicates the default value FROM which
 * the system will start to discard messages, so as an example:
 *
 *  - if set to SCSC_DBG1 (7) every message with a dbglvl >= 7 will be discarded
 *  - if set to SCSC_FULL_DEBUG (11) every message is logged
 *
 * Name, Default DROPLEVEL , FilterTag
 */
ADD_DEBUG_MODULE_PARAM(binary,  SCSC_FULL_DEBUG, BINARY);
ADD_DEBUG_MODULE_PARAM(bin_wifi_ctrl_rx,  SCSC_FULL_DEBUG, BIN_WIFI_CTRL_RX);
ADD_DEBUG_MODULE_PARAM(bin_wifi_data_rx,  SCSC_FULL_DEBUG, BIN_WIFI_DATA_RX);
ADD_DEBUG_MODULE_PARAM(bin_wifi_ctrl_tx,  SCSC_FULL_DEBUG, BIN_WIFI_CTRL_TX);
ADD_DEBUG_MODULE_PARAM(bin_wifi_data_tx,  SCSC_FULL_DEBUG, BIN_WIFI_DATA_TX);
ADD_DEBUG_MODULE_PARAM(wifi_rx, SCSC_FULL_DEBUG, WIFI_RX);
ADD_DEBUG_MODULE_PARAM(wifi_tx, SCSC_FULL_DEBUG, WIFI_TX);
ADD_DEBUG_MODULE_PARAM(bt_common, SCSC_FULL_DEBUG, BT_COMMON);
ADD_DEBUG_MODULE_PARAM(bt_h4,   SCSC_FULL_DEBUG, BT_H4);
ADD_DEBUG_MODULE_PARAM(bt_fw,   SCSC_FULL_DEBUG, BT_FW);
ADD_DEBUG_MODULE_PARAM(bt_rx,   SCSC_FULL_DEBUG, BT_RX);
ADD_DEBUG_MODULE_PARAM(bt_tx,   SCSC_FULL_DEBUG, BT_TX);
ADD_DEBUG_MODULE_PARAM(cpktbuff,   SCSC_DBG4, CPKTBUFF);
ADD_DEBUG_MODULE_PARAM(fw_load,   SCSC_FULL_DEBUG, FW_LOAD);
ADD_DEBUG_MODULE_PARAM(fw_panic,   SCSC_FULL_DEBUG, FW_PANIC);
ADD_DEBUG_MODULE_PARAM(gdb_trans,   SCSC_DBG1, GDB_TRANS);
ADD_DEBUG_MODULE_PARAM(mif,   SCSC_FULL_DEBUG, MIF);
ADD_DEBUG_MODULE_PARAM(clk20,   SCSC_FULL_DEBUG, CLK20);
ADD_DEBUG_MODULE_PARAM(clk20_test, SCSC_FULL_DEBUG, CLK20_TEST);
ADD_DEBUG_MODULE_PARAM(fm,   SCSC_FULL_DEBUG, FM);
ADD_DEBUG_MODULE_PARAM(fm_test, SCSC_FULL_DEBUG, FM_TEST);
ADD_DEBUG_MODULE_PARAM(mx_file,   SCSC_FULL_DEBUG, MX_FILE);
ADD_DEBUG_MODULE_PARAM(mx_fw,   SCSC_FULL_DEBUG, MX_FW);
ADD_DEBUG_MODULE_PARAM(mx_sampler,   SCSC_FULL_DEBUG, MX_SAMPLER);
ADD_DEBUG_MODULE_PARAM(mxlog_trans,   SCSC_DBG1, MXLOG_TRANS);
ADD_DEBUG_MODULE_PARAM(mxman,   SCSC_FULL_DEBUG, MXMAN);
ADD_DEBUG_MODULE_PARAM(mxman_test, SCSC_FULL_DEBUG, MXMAN_TEST);
ADD_DEBUG_MODULE_PARAM(mxmgt_trans,   SCSC_FULL_DEBUG, MXMGT_TRANS);
ADD_DEBUG_MODULE_PARAM(mx_mmap,   SCSC_DBG1, MX_MMAP);
ADD_DEBUG_MODULE_PARAM(mx_proc,   SCSC_FULL_DEBUG, MX_PROC);
ADD_DEBUG_MODULE_PARAM(panic_mon, SCSC_FULL_DEBUG, PANIC_MON);
ADD_DEBUG_MODULE_PARAM(pcie_mif, SCSC_FULL_DEBUG, PCIE_MIF);
ADD_DEBUG_MODULE_PARAM(plat_mif, SCSC_DBG1, PLAT_MIF);
ADD_DEBUG_MODULE_PARAM(kic_common, SCSC_FULL_DEBUG, KIC_COMMON);
ADD_DEBUG_MODULE_PARAM(wlbtd, SCSC_FULL_DEBUG, WLBTD);
ADD_DEBUG_MODULE_PARAM(wlog, SCSC_DEBUG, WLOG);
ADD_DEBUG_MODULE_PARAM(lerna, SCSC_FULL_DEBUG, LERNA);
ADD_DEBUG_MODULE_PARAM(mxcfg, SCSC_FULL_DEBUG, MX_CFG);
#ifdef CONFIG_SCSC_DEBUG_COMPATIBILITY
ADD_DEBUG_MODULE_PARAM(init_deinit,  SCSC_FULL_DEBUG, SLSI_INIT_DEINIT);
ADD_DEBUG_MODULE_PARAM(netdev,  SCSC_DBG4, SLSI_NETDEV);
ADD_DEBUG_MODULE_PARAM(cfg80211,  SCSC_FULL_DEBUG, SLSI_CFG80211);
ADD_DEBUG_MODULE_PARAM(mlme,  SCSC_FULL_DEBUG, SLSI_MLME);
ADD_DEBUG_MODULE_PARAM(summary_frames,  SCSC_FULL_DEBUG, SLSI_SUMMARY_FRAMES);
ADD_DEBUG_MODULE_PARAM(hydra,  SCSC_FULL_DEBUG, SLSI_HYDRA);
ADD_DEBUG_MODULE_PARAM(tx,  SCSC_FULL_DEBUG, SLSI_TX);
ADD_DEBUG_MODULE_PARAM(rx,  SCSC_FULL_DEBUG, SLSI_RX);
ADD_DEBUG_MODULE_PARAM(udi,  SCSC_DBG4, SLSI_UDI);
ADD_DEBUG_MODULE_PARAM(wifi_fcq,  SCSC_DBG4, SLSI_WIFI_FCQ);
ADD_DEBUG_MODULE_PARAM(hip,  SCSC_FULL_DEBUG, SLSI_HIP);
ADD_DEBUG_MODULE_PARAM(hip_init_deinit,  SCSC_FULL_DEBUG, SLSI_HIP_INIT_DEINIT);
ADD_DEBUG_MODULE_PARAM(hip_fw_dl,  SCSC_FULL_DEBUG, SLSI_HIP_FW_DL);
ADD_DEBUG_MODULE_PARAM(hip_sdio_op,  SCSC_FULL_DEBUG, SLSI_HIP_SDIO_OP);
ADD_DEBUG_MODULE_PARAM(hip_ps,  SCSC_FULL_DEBUG, SLSI_HIP_PS);
ADD_DEBUG_MODULE_PARAM(hip_th,  SCSC_FULL_DEBUG, SLSI_HIP_TH);
ADD_DEBUG_MODULE_PARAM(hip_fh,  SCSC_FULL_DEBUG, SLSI_HIP_FH);
ADD_DEBUG_MODULE_PARAM(hip_sig,  SCSC_FULL_DEBUG, SLSI_HIP_SIG);
ADD_DEBUG_MODULE_PARAM(func_trace,  SCSC_FULL_DEBUG, SLSI_FUNC_TRACE);
ADD_DEBUG_MODULE_PARAM(test,  SCSC_FULL_DEBUG, SLSI_TEST);
ADD_DEBUG_MODULE_PARAM(src_sink,  SCSC_FULL_DEBUG, SLSI_SRC_SINK);
ADD_DEBUG_MODULE_PARAM(fw_test,  SCSC_DBG4, SLSI_FW_TEST);
ADD_DEBUG_MODULE_PARAM(rx_ba,  SCSC_FULL_DEBUG, SLSI_RX_BA);
ADD_DEBUG_MODULE_PARAM(tdls,  SCSC_FULL_DEBUG, SLSI_TDLS);
ADD_DEBUG_MODULE_PARAM(gscan,  SCSC_FULL_DEBUG, SLSI_GSCAN);
ADD_DEBUG_MODULE_PARAM(mbulk,  SCSC_DBG1, SLSI_MBULK);
ADD_DEBUG_MODULE_PARAM(smapper,  SCSC_DBG1, SLSI_SMAPPER);
ADD_DEBUG_MODULE_PARAM(flowc, SCSC_FULL_DEBUG, SLSI_FLOWC);
#endif
ADD_DEBUG_MODULE_PARAM(test_me, SCSC_FULL_DEBUG, TEST_ME);

/* Extend this list when you add ADD_DEBUG_MODULE_PARAM, above.
 * You must also extend "enum scsc_logring_tags"
 */
static int *scsc_droplevels[MAX_TAG + 1] = {
	&scsc_droplevel_binary,
	&scsc_droplevel_bin_wifi_ctrl_rx,
	&scsc_droplevel_bin_wifi_data_rx,
	&scsc_droplevel_bin_wifi_ctrl_tx,
	&scsc_droplevel_bin_wifi_data_tx,
	&scsc_droplevel_wlbt,
	&scsc_droplevel_wifi_rx,
	&scsc_droplevel_wifi_tx,
	&scsc_droplevel_bt_common,
	&scsc_droplevel_bt_h4,
	&scsc_droplevel_bt_fw,
	&scsc_droplevel_bt_rx,
	&scsc_droplevel_bt_tx,
	&scsc_droplevel_cpktbuff,
	&scsc_droplevel_fw_load,
	&scsc_droplevel_fw_panic,
	&scsc_droplevel_gdb_trans,
	&scsc_droplevel_mif,
	&scsc_droplevel_clk20,
	&scsc_droplevel_clk20_test,
	&scsc_droplevel_fm,
	&scsc_droplevel_fm_test,
	&scsc_droplevel_mx_file,
	&scsc_droplevel_mx_fw,
	&scsc_droplevel_mx_sampler,
	&scsc_droplevel_mxlog_trans,
	&scsc_droplevel_mxman,
	&scsc_droplevel_mxman_test,
	&scsc_droplevel_mxmgt_trans,
	&scsc_droplevel_mx_mmap,
	&scsc_droplevel_mx_proc,
	&scsc_droplevel_panic_mon,
	&scsc_droplevel_pcie_mif,
	&scsc_droplevel_plat_mif,
	&scsc_droplevel_kic_common,
	&scsc_droplevel_wlbtd,
	&scsc_droplevel_wlog,
	&scsc_droplevel_lerna,
	&scsc_droplevel_mxcfg,
#ifdef CONFIG_SCSC_DEBUG_COMPATIBILITY
	&scsc_droplevel_init_deinit,
	&scsc_droplevel_netdev,
	&scsc_droplevel_cfg80211,
	&scsc_droplevel_mlme,
	&scsc_droplevel_summary_frames,
	&scsc_droplevel_hydra,
	&scsc_droplevel_tx,
	&scsc_droplevel_rx,
	&scsc_droplevel_udi,
	&scsc_droplevel_wifi_fcq,
	&scsc_droplevel_hip,
	&scsc_droplevel_hip_init_deinit,
	&scsc_droplevel_hip_fw_dl,
	&scsc_droplevel_hip_sdio_op,
	&scsc_droplevel_hip_ps,
	&scsc_droplevel_hip_th,
	&scsc_droplevel_hip_fh,
	&scsc_droplevel_hip_sig,
	&scsc_droplevel_func_trace,
	&scsc_droplevel_test,
	&scsc_droplevel_src_sink,
	&scsc_droplevel_fw_test,
	&scsc_droplevel_rx_ba,
	&scsc_droplevel_tdls,
	&scsc_droplevel_gscan,
	&scsc_droplevel_mbulk,
	&scsc_droplevel_flowc,
	&scsc_droplevel_smapper,
#endif
	&scsc_droplevel_test_me, /* Must be last */
};

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
static int logring_collect(struct scsc_log_collector_client *collect_client, size_t size)
{
	int ret = 0, saved_droplevel;

	if (!the_ringbuf)
		return 0;

	/**
	 * Inhibit logring during collection overriding with scsc_droplevel_all
	 */
	saved_droplevel = scsc_droplevel_all;
	scsc_droplevel_all = DEFAULT_DROP_ALL;

	/* Write buffer */
	ret = scsc_log_collector_write(the_ringbuf->buf, the_ringbuf->bsz, 1);

	scsc_droplevel_all = saved_droplevel;

	return ret;
}
#endif

static int scsc_reset_all_droplevels_to_set_param_cb(const char *val,
						     const struct kernel_param *kp)
{
	int ret = 0, i = 0;
	long rval = 0;

	if (!val)
		return -EINVAL;
	ret = kstrtol(val, 10, &rval);
	if (!ret) {
		if (rval < 0)
			return -EINVAL;
		scsc_droplevel_wlbt = (int)rval;
		for (i = 0; i < ARRAY_SIZE(scsc_droplevels); i++)
			*scsc_droplevels[i] = (int)rval;
		scsc_reset_all_droplevels_to = (int)rval;
		scsc_printk_tag(FORCE_PRK, NO_TAG,
				KERN_INFO"Reset ALL droplevels to %d\n",
				scsc_reset_all_droplevels_to);
	}
	return ret;
}

static struct kernel_param_ops scsc_reset_droplvl_ops = {
	.set = scsc_reset_all_droplevels_to_set_param_cb,
	.get = NULL,
};
module_param_cb(scsc_reset_all_droplevels_to, &scsc_reset_droplvl_ops,
		NULL, 0200);
MODULE_PARM_DESC(scsc_reset_all_droplevels_to,
		 "Reset ALL droplevels to the requested value. Effective @run-time.");


/* SCSC_PRINTK API and Helpers */
static inline int get_debug_level(const char *fmt)
{
	int level;

	if (fmt && *fmt == SCSC_SOH && *(fmt + 1))
		level = *(fmt + 1) - '0';
	else
		level = default_dbglevel;
	return level;
}

static inline void drop_message_level_macro(const char *fmt, char **msg)
{
	if (fmt && *fmt == SCSC_SOH && *(fmt + 1)) {
		if (msg)
			*msg = (char *)(fmt + 2);
	} else if (msg) {
		*msg = (char *)fmt;
	}
}

/**
 * The workhorse function that receiving a droplevel to enforce, and a pair
 * format_string/va_list decides what to do:
 *  - drop
 *  OR
 *  - insert into ring buffer accounting for wrapping
 *
 *  ... then wakes up any waiting reading process
 */
static inline int _scsc_printk(int level, int tag,
			       const char *fmt, va_list args)
{
	int  written = 0;
	char *msg_head = NULL;

	if (!initialized || !enable || !fmt ||
	    ((scsc_droplevel_all < 0 && level >= *scsc_droplevels[tag]) ||
	     (scsc_droplevel_all >= 0 && level >= scsc_droplevel_all)))
		return written;
	drop_message_level_macro(fmt, &msg_head);
	written = push_record_string(the_ringbuf, tag, level,
				     prepend_header, msg_head, args);
	return written;
}

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
#define SCSC_LVL2MEMLOG(lv)	(((lv) < 3) ? MEMLOG_LEVEL_EMERG : ((lv) < 7) ? (lv) - 2 : MEMLOG_LEVEL_DEBUG)
#define IS_MEMLOG_ALLOWED(ff, level, memlog_lv) \
	((ff) == FORCE_PRK || \
	((ff) != NO_ECHO_PRK && (level) < scsc_redirect_to_printk_droplvl) || \
	((ff) != NO_ECHO_PRK && SCSC_LVL2MEMLOG(level) <= (memlog_lv)))

struct memlog_obj *print_obj;
extern const char *tagstr[MAX_TAG + 1];

static inline bool memlog_get_obj(void)
{
	struct memlog *memlog_desc = NULL;

	memlog_desc = memlog_get_desc("WB_LOG");
	if (!memlog_desc)
		return false;

	print_obj = memlog_get_obj_by_name(memlog_desc, "ker_mem");
	if (!print_obj) {
		struct memlog_obj *file_obj = memlog_alloc_file(memlog_desc, "ker_fil", SZ_1M, 1, 1000, 1);

		if (!file_obj)
			return false;

		print_obj = memlog_alloc_printf(memlog_desc, SZ_512K, file_obj, "ker_mem", false);
		if (!print_obj)
			return false;
	}

	return true;
}

static inline
void memlog_out_string(int level, int tag, const char *fmt, va_list args,
			int force)
{
	unsigned long flags;
	struct va_format vaf;
	u8 ctx;
	u8 core;
	char *msg_head = NULL;

	if (!enable)
		return;

	if (print_obj == NULL)
		if (!memlog_get_obj())
			return;

	if (!IS_MEMLOG_ALLOWED(force, level, print_obj->log_level))
		return;

	drop_message_level_macro(fmt, &msg_head);
	vaf.fmt = msg_head;
	vaf.va = &args;

	local_irq_save(flags);
	ctx = in_interrupt() ? (in_softirq() ? 'S' : 'I') : 'P';
	core = smp_processor_id();
	local_irq_restore(flags);

	if (print_obj)
		memlog_write_printf(print_obj, SCSC_LVL2MEMLOG(level),
			"<%d> [c%d] [%c] [%s] :: %pV",
			level, core, ctx, tagstr[tag], &vaf);
	else
		pr_notice("Samlog: print_obj is not valid\n");
}

static inline
void memlog_out_bin(int level, int tag, const void *start, size_t len,
			int force)
{
	const u8 *ptr = start;
	int i, linelen, rowsize = 32, remaining = len;
	unsigned char linebuf[64 * 3 + 1];
	unsigned long flags;
	u8 ctx;
	u8 core;

	if (!enable)
		return;

	if (print_obj == NULL)
		if (!memlog_get_obj())
			return;

	if (!IS_MEMLOG_ALLOWED(force, level, print_obj->log_level))
		return;

	local_irq_save(flags);
	ctx = in_interrupt() ? (in_softirq() ? 'S' : 'I') : 'P';
	core = smp_processor_id();
	local_irq_restore(flags);

	for (i = 0; i < len; i += rowsize) {
		linelen = min(remaining, rowsize);
		remaining -= rowsize;

		hex_dump_to_buffer(ptr + i, linelen, rowsize, 1,
				   linebuf, sizeof(linebuf), false);

		if (print_obj)
			memlog_write_printf(print_obj, SCSC_LVL2MEMLOG(level),
				"<%d> [c%d] [%c] [%s] "SCSC_PREFIX"SCSC_HEX->|%s\n",
				level, core, ctx, tagstr[tag], linebuf);
		else
			pr_notice("Samlog: print_obj is not valid\n");
	}
}
#endif

/**
 * Embeds the filtering behaviour towards std kenrel ring buffer for
 * non binary stuff, and decides what to do based on current user config.
 * Note that printk redirect doplevel filter is now completely disjoint
 * from normal LogRing droplevel filters.
 */
static inline
void handle_klogbuf_out_string(int level, struct device *dev, int tag,
			       const char *fmt, va_list args, int force)
{
	if (IS_PRINTK_REDIRECT_ALLOWED(force, level, tag)) {
		if (!dev)
			vprintk_emit(0, level, NULL, 0, fmt, args);
		else
			dev_vprintk_emit(level, dev, fmt, args);
	}
}

static const char * const map2kern[] = {
	KERN_EMERG,
	KERN_ALERT,
	KERN_CRIT,
	KERN_ERR,
	KERN_WARNING,
	KERN_NOTICE,
	KERN_INFO,
	KERN_DEBUG
};

/**
 * Embeds the filtering behaviour towards std kenrel ring buffer for
 * BINARY stuff, and decides what to do based on current user config.
 * Note that printk redirect doplevel filter is now completely disjoint
 * from normal LogRing droplevel filters.
 */
static inline
void handle_klogbuf_out_binary(int level, int tag, const void *start,
			       size_t len, int force)
{
	if (IS_PRINTK_REDIRECT_ALLOWED(force, level, tag)) {
		if (level < SCSC_MIN_DBG || level >= ARRAY_SIZE(map2kern))
			level = ARRAY_SIZE(map2kern) - 1;
		print_hex_dump(map2kern[level], SCSC_PREFIX"SCSC_HEX->|",
			       DUMP_PREFIX_NONE, 16, 1, start, len, false);
	}
}

/*
 * scsc_printk - it's main API entry to the event logging mechanism. Prints
 * the specified format string to the underlying ring_buffer, injecting
 * timestamp and context information at the start of the line while
 * classifying and filtering the message suing the specified TAG identifier.
 *
 * This function assumes that you'll never write a line longer than
 * BASE_SPARE_SZ bytes; if this limit is obeyed any input string is correctly
 * wrapped when placed at the end of the buffer. Any longer line will be
 * trucated.
 * This function recognize Kernel style debug level KERN_, checking the FIRST
 * byte for ASCII SOH in order to recognize if some printk style kernel debug
 * level has been specified.
 * SO you can use KERN_INFO KERN_ERR etc etc INLINE macros to specify the
 * desired debug level: that will be checked against the droplevel specified.
 * If NOT specified a default debug level is assigned following what specified
 * in module parameter default_dbglevel.
 *
 * It's usually NOT used directly but through the means of utility macros that
 * can be easily compiled out in production builds.
 */
int scsc_printk_tag(int force, int tag, const char *fmt, ...)
{
	int     ret = 0, level = 0;
	va_list args;

	/* Cannot use BINARY tag with strings logging */
	if (tag < NO_TAG || tag > MAX_TAG)
		return ret;
	level = get_debug_level(fmt);
	if ((in_interrupt() && level >= scsc_droplevel_atomic))
		return ret;
	va_start(args, fmt);
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	memlog_out_string(level, tag, fmt, args, force);
#endif
	handle_klogbuf_out_string(level, NULL, tag, fmt, args, force);
	va_end(args);
	/* restart varargs */
	va_start(args, fmt);
	ret = _scsc_printk(level, tag, fmt, args);
	va_end(args);
	return ret;
}
EXPORT_SYMBOL(scsc_printk_tag);


/**
 * This is a variation on the main API that allows to specify loglevel
 * by number.
 */
int scsc_printk_tag_lvl(int tag, int level, const char *fmt, ...)
{
	int     ret = 0;
	va_list args;

	/* Cannot use BINARY tag with strings logging */
	if (tag < NO_TAG || tag > MAX_TAG)
		return ret;
	if ((in_interrupt() && level >= scsc_droplevel_atomic))
		return ret;
	va_start(args, fmt);
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	memlog_out_string(level, tag, fmt, args, NO_FORCE_PRK);
#endif
	handle_klogbuf_out_string(level, NULL, tag, fmt, args, NO_FORCE_PRK);
	va_end(args);
	/* restart varargs */
	va_start(args, fmt);
	ret = _scsc_printk(level, tag, fmt, args);
	va_end(args);
	return ret;
}
EXPORT_SYMBOL(scsc_printk_tag_lvl);


/**
 * This is a variation on the main API that allows to specify a
 * struct device reference.
 */
int scsc_printk_tag_dev(int force, int tag, struct device *dev,
			 const char *fmt, ...)
{
	int     ret = 0, level = 0;
	va_list args;

	/* Cannot use BINARY tag with strings logging */
	if (tag < NO_TAG || tag > MAX_TAG)
		return ret;
	level = get_debug_level(fmt);
	if ((in_interrupt() && level >= scsc_droplevel_atomic))
		return ret;
	va_start(args, fmt);
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	memlog_out_string(level, tag, fmt, args, force);
#endif
	handle_klogbuf_out_string(level, dev, tag, fmt, args, force);
	va_end(args);
	/* restart varargs */
	va_start(args, fmt);
	ret = _scsc_printk(level, tag, fmt, args);
	va_end(args);
	return ret;
}
EXPORT_SYMBOL(scsc_printk_tag_dev);

/**
 * This is a variation on the main API that allows to specify a
 * struct device reference and an explicit numerical debug level.
 */
int scsc_printk_tag_dev_lvl(int force, int tag, struct device *dev,
			    int level, const char *fmt, ...)
{
	int     ret = 0;
	va_list args;

	/* Cannot use BINARY tag with strings logging */
	if (tag < NO_TAG || tag > MAX_TAG)
		return ret;
	if ((in_interrupt() && level >= scsc_droplevel_atomic))
		return ret;
	va_start(args, fmt);
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	memlog_out_string(level, tag, fmt, args, force);
#endif
	handle_klogbuf_out_string(level, dev, tag, fmt, args, force);
	va_end(args);
	/* restart varargs */
	va_start(args, fmt);
	ret = _scsc_printk(level, tag, fmt, args);
	va_end(args);
	return ret;
}
EXPORT_SYMBOL(scsc_printk_tag_dev_lvl);

/**
 * This is a variation on the main API used to push binary blob into the ring.
 */
int scsc_printk_bin(int force, int tag, int dlev, const void *start, size_t len)
{
	int ret = 0;
	/* Cannot use NON BINARY tag with strings logging
	 * or NULLs start/len
	 */
	if (!start || !len || tag < FIRST_BIN_TAG || tag > LAST_BIN_TAG)
		return ret;
	dlev = (dlev >= 0) ? dlev : default_dbglevel;
	if ((in_interrupt() && dlev >= scsc_droplevel_atomic))
		return ret;
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	memlog_out_bin(dlev, tag, start, len, force);
#endif
	handle_klogbuf_out_binary(dlev, tag, start, len, force);
	/* consider proper tag droplevel */
	if (!initialized || !enable || !start ||
	    (scsc_droplevel_all < 0 && *scsc_droplevels[tag] <= dlev) ||
	    (scsc_droplevel_all >= 0 && scsc_droplevel_all <= dlev))
		return ret;
	ret = push_record_blob(the_ringbuf, tag, dlev,
			       prepend_header, start, len);
	return ret;
}
EXPORT_SYMBOL(scsc_printk_bin);

/*
 * This is a very basic mechanism to have implement the dynamic switch
 * for one user (currently WLAN). If multiple users are
 * required to use the dynamic logring switch, a new registration
 * mechanism based on requests and use_count should be implemented to avoid one service
 * re-enabling logring when some other has requested not to do so.
 */
int scsc_logring_enable(bool logging_enable)
{
	scsc_printk_tag(FORCE_PRK, NO_TAG, "scsc_logring %s\n", logging_enable ? "enable" : "disable");
	/* User has requested to disable logring */
	if (!logging_enable && enable) {
		cached_enable = true;
		enable = 0;
		scsc_printk_tag(FORCE_PRK, NO_TAG, "Logring disabled\n");
	} else if (logging_enable && cached_enable) {
		cached_enable = false;
		enable = 1;
		scsc_printk_tag(FORCE_PRK, NO_TAG, "Logring re-enabled\n");
	} else {
		scsc_printk_tag(FORCE_PRK, NO_TAG, "Ignored\n");
	}
	return 0;
}
EXPORT_SYMBOL(scsc_logring_enable);


MODULE_DESCRIPTION("SCSC Event Logger");
MODULE_AUTHOR("SLSI");
MODULE_LICENSE("GPL and additional rights");
