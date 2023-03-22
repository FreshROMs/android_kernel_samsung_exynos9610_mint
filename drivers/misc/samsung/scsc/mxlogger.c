/****************************************************************************
 *
 * Copyright (c) 2014 - 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/** Implements */
#include "mxlogger.h"

/** Uses */
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/version.h>
#include <scsc/scsc_logring.h>
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
#include <scsc/scsc_log_collector.h>
#endif

#include "srvman.h"
#include "scsc_mif_abs.h"
#include "miframman.h"
#include "mifintrbit.h"
#include "mxmgmt_transport.h"

static bool mxlogger_disabled;
module_param(mxlogger_disabled, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mxlogger_disabled, "Disable MXLOGGER Configuration. Effective only at next WLBT boot.");

static bool mxlogger_manual_layout;
module_param(mxlogger_manual_layout, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mxlogger_manual_layout, "User owns the buffer layout. Only sync buffer will be allocated");

static int mxlogger_manual_total_mem = MXL_POOL_SZ - MXLOGGER_SYNC_SIZE - sizeof(struct mxlogger_config_area);
module_param(mxlogger_manual_total_mem , int, S_IRUGO);
MODULE_PARM_DESC(mxlogger_manual_total_mem, "Available memory when mxlogger_manual_layout is enabled");

static int mxlogger_manual_imp;
module_param(mxlogger_manual_imp , int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mxlogger_manual_imp, "size for IMP buffer when mxlogger_manual_layout is enabled");

static int mxlogger_manual_rsv_common;
module_param(mxlogger_manual_rsv_common , int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mxlogger_manual_rsv_common, "size for RSV COMMON buffer when mxlogger_manual_layout is enabled");

static int mxlogger_manual_rsv_bt;
module_param(mxlogger_manual_rsv_bt , int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mxlogger_manual_rsv_bt, "size for RSV BT buffer when mxlogger_manual_layout is enabled");

static int mxlogger_manual_rsv_wlan = MXL_POOL_SZ - MXLOGGER_SYNC_SIZE - sizeof(struct mxlogger_config_area);
module_param(mxlogger_manual_rsv_wlan , int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mxlogger_manual_rsv_wlan, "size for RSV WLAN buffer when mxlogger_manual_layout is enabled");

static int mxlogger_manual_rsv_radio;
module_param(mxlogger_manual_rsv_radio , int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mxlogger_manual_rsv_radio, "size for RSV RADIO buffer when mxlogger_manual_layout is enabled");

static int mxlogger_manual_mxlog;
module_param(mxlogger_manual_mxlog , int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mmxlogger_manual_mxlog, "size for MXLOG buffer when mxlogger_manual_layout is enabled");

static int mxlogger_manual_udi;
module_param(mxlogger_manual_udi , int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mxlogger_manual_udi, "size for UDI buffer when mxlogger_manual_layout is enabled");

#if 0 //TODO
bool mxlogger_set_enabled_status(bool enable)
{
	mxlogger_disabled = !enable;

	SCSC_TAG_INFO(MXMAN, "MXLOGGER has been NOW %sABLED. Effective at next WLBT boot.\n",
		      mxlogger_disabled ? "DIS" : "EN");

	return mxlogger_disabled;
}
EXPORT_SYMBOL(mxlogger_set_enabled_status);
#endif

static bool mxlogger_forced_to_host;

static void update_fake_observer(void)
{
	static bool mxlogger_fake_observers_registered;

	if (mxlogger_forced_to_host) {
		if (!mxlogger_fake_observers_registered) {
			mxlogger_register_global_observer("FAKE_OBSERVER");
			mxlogger_fake_observers_registered = true;
		}
		SCSC_TAG_INFO(MXMAN, "MXLOGGER is now FORCED TO HOST.\n");
	} else {
		if (mxlogger_fake_observers_registered) {
			mxlogger_unregister_global_observer("FAKE_OBSERVER");
			mxlogger_fake_observers_registered = false;
		}
		SCSC_TAG_INFO(MXMAN, "MXLOGGER is now operating NORMALLY.\n");
	}
}

static int mxlogger_force_to_host_set_param_cb(const char *val,
					       const struct kernel_param *kp)
{
	bool nval;

	if (!val || strtobool(val, &nval))
		return -EINVAL;

	if (mxlogger_forced_to_host ^ nval) {
		mxlogger_forced_to_host = nval;
		update_fake_observer();
	}
	return 0;
}

/**
 * As described in struct kernel_param+ops the _get method:
 * -> returns length written or -errno.  Buffer is 4k (ie. be short!)
 */
static int mxlogger_force_to_host_get_param_cb(char *buffer,
					       const struct kernel_param *kp)
{
	return sprintf(buffer, "%c", mxlogger_forced_to_host ? 'Y' : 'N');
}

static struct kernel_param_ops mxlogger_force_to_host_ops = {
	.set = mxlogger_force_to_host_set_param_cb,
	.get = mxlogger_force_to_host_get_param_cb,
};
module_param_cb(mxlogger_force_to_host, &mxlogger_force_to_host_ops, NULL, 0644);
MODULE_PARM_DESC(mxlogger_force_to_host, "Force mxlogger to redirect to Host all the time, using a fake observer.");

/**
 * Observers of log material could come and go before mxman and mxlogger
 * are initialized and started...so we keep this stuff here out of mxman,
 * but all the lifecycle of mxlogger should be reviewed.
 */
static u8 active_global_observers;
static DEFINE_MUTEX(global_lock);

struct mxlogger_node {
	struct list_head list;
	struct mxlogger *mxl;
};
static struct mxlogger_list { struct list_head list; } mxlogger_list = {
	.list = LIST_HEAD_INIT(mxlogger_list.list)
};

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
static int mxlogger_collect_init(struct scsc_log_collector_client *collect_client);
static int mxlogger_collect(struct scsc_log_collector_client *collect_client, size_t size);
static int mxlogger_collect_end(struct scsc_log_collector_client *collect_client);

/* Collect client registration SYNC buffer */
/* SYNC - SHOULD BE THE FIRST CHUNK TO BE CALLED - SO USE THE INIT/END ON THIS CLIENT */
static struct scsc_log_collector_client mxlogger_collect_client_sync = {
	.name = "Sync",
	.type = SCSC_LOG_CHUNK_SYNC,
	.collect_init = mxlogger_collect_init,
	.collect = mxlogger_collect,
	.collect_end = mxlogger_collect_end,
	.prv = NULL,
};

/* Collect client registration IMP buffer */
static struct scsc_log_collector_client mxlogger_collect_client_imp = {
	.name = "Important",
	.type = SCSC_LOG_CHUNK_IMP,
	.collect_init = NULL,
	.collect = mxlogger_collect,
	.collect_end = NULL,
	.prv = NULL,
};

static struct scsc_log_collector_client mxlogger_collect_client_rsv_common = {
	.name = "Rsv_common",
	.type = SCSC_LOG_RESERVED_COMMON,
	.collect_init = NULL,
	.collect = mxlogger_collect,
	.collect_end = NULL,
	.prv = NULL,
};

static struct scsc_log_collector_client mxlogger_collect_client_rsv_bt = {
	.name = "Rsv_bt",
	.type = SCSC_LOG_RESERVED_BT,
	.collect_init = NULL,
	.collect = mxlogger_collect,
	.collect_end = NULL,
	.prv = NULL,
};

static struct scsc_log_collector_client mxlogger_collect_client_rsv_wlan = {
	.name = "Rsv_wlan",
	.type = SCSC_LOG_RESERVED_WLAN,
	.collect_init = NULL,
	.collect = mxlogger_collect,
	.collect_end = NULL,
	.prv = NULL,
};

static struct scsc_log_collector_client mxlogger_collect_client_rsv_radio = {
	.name = "Rsv_radio",
	.type = SCSC_LOG_RESERVED_RADIO,
	.collect_init = NULL,
	.collect = mxlogger_collect,
	.collect_end = NULL,
	.prv = NULL,
};
/* Collect client registration MXL buffer */
static struct scsc_log_collector_client mxlogger_collect_client_mxl = {
	.name = "MXL",
	.type = SCSC_LOG_CHUNK_MXL,
	.collect_init = NULL,
	.collect = mxlogger_collect,
	.collect_end = NULL,
	.prv = NULL,
};

/* Collect client registration MXL buffer */
static struct scsc_log_collector_client mxlogger_collect_client_udi = {
	.name = "UDI",
	.type = SCSC_LOG_CHUNK_UDI,
	.collect_init = NULL,
	.collect = mxlogger_collect,
	.collect_end = NULL,
	.prv = NULL,
};
#endif

static const char * const mxlogger_buf_name[] = {
	"syn", "imp", "rsv_common", "rsv_bt", "rsv_wlan", "rsv_radio", "mxl", "udi"
};

static void mxlogger_message_handler(const void *message, void *data)
{
	struct mxlogger		__attribute__((unused)) *mxlogger = (struct mxlogger *)data;
	const struct log_msg_packet	*msg = message;
	u16 reason_code;

	switch (msg->msg) {
	case MM_MXLOGGER_INITIALIZED_EVT:
		SCSC_TAG_INFO(MXMAN, "MXLOGGER Initialized.\n");
		mxlogger->initialized = true;
		complete(&mxlogger->rings_serialized_ops);
		break;
	case MM_MXLOGGER_STARTED_EVT:
		SCSC_TAG_INFO(MXMAN, "MXLOGGER:: RINGS Enabled.\n");
		mxlogger->enabled = true;
		complete(&mxlogger->rings_serialized_ops);
		break;
	case MM_MXLOGGER_STOPPED_EVT:
		SCSC_TAG_INFO(MXMAN, "MXLOGGER:: RINGS Disabled.\n");
		mxlogger->enabled = false;
		complete(&mxlogger->rings_serialized_ops);
		break;
	case MM_MXLOGGER_COLLECTION_FW_REQ_EVT:
		/* If arg is zero, FW is using the 16bit reason code API */
		/* therefore, the reason code is in the payload */
		if (msg->arg == 0x00)
			memcpy(&reason_code, &msg->payload[0], sizeof(u16));
		else
			/* old API */
			reason_code = msg->arg;
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
		SCSC_TAG_INFO(MXMAN, "MXLOGGER:: FW requested collection - Reason code:0x%04x\n", reason_code);
		scsc_log_collector_schedule_collection(SCSC_LOG_FW, reason_code);
#endif
		break;
	default:
		SCSC_TAG_WARNING(MXMAN,
				 "Received UNKNOWN msg on MMTRANS_CHAN_ID_MAXWELL_LOGGING -- msg->msg=%d\n",
				 msg->msg);
		break;
	}
}

static int __mxlogger_generate_sync_record(struct mxlogger *mxlogger, enum mxlogger_sync_event event)
{
	struct mxlogger_sync_record *sync_r_mem;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	struct timespec64 ts;
#else
	struct timeval t;
#endif
	struct log_msg_packet msg = {};
	unsigned long int jd;
	void *mem;
	ktime_t t1, t2;

	/* Assume mxlogger->lock mutex is held */
	if (!mxlogger || !mxlogger->configured)
		return -EIO;

	msg.msg = MM_MXLOGGER_SYNC_RECORD;
	msg.arg = MM_MXLOGGER_SYNC_INDEX;
	memcpy(&msg.payload, &mxlogger->sync_buffer_index, sizeof(mxlogger->sync_buffer_index));

	/* Get the pointer from the index of the sync array */
	mem =  mxlogger->mem_sync_buf + mxlogger->sync_buffer_index * sizeof(struct mxlogger_sync_record);
	sync_r_mem = (struct mxlogger_sync_record *)mem;
	/* Write values in record as FW migth be doing sanity checks */
	sync_r_mem->tv_sec = 1;
	sync_r_mem->tv_usec = 1;
	sync_r_mem->kernel_time = 1;
	sync_r_mem->sync_event = event;
	sync_r_mem->fw_time = 0;
	sync_r_mem->fw_wrap = 0;


	SCSC_TAG_INFO(MXMAN, "Get FW time\n");
	preempt_disable();
	/* set the tight loop timeout - we do not require precission but something to not
	 * loop forever
	 */
	jd = jiffies + msecs_to_jiffies(20);
	/* Send the msg as fast as possible */
	mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(mxlogger->mx),
			      MMTRANS_CHAN_ID_MAXWELL_LOGGING,
			      &msg, sizeof(msg));
	t1 = ktime_get();
	/* Tight loop to read memory */
	while (time_before(jiffies, jd) && sync_r_mem->fw_time == 0 && sync_r_mem->fw_wrap == 0)
		;
	t2 = ktime_get();
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	ktime_get_real_ts64(&ts);
#else
	do_gettimeofday(&t);
#endif
	preempt_enable();

	/* Do the processing */
	if (sync_r_mem->fw_wrap == 0 && sync_r_mem->fw_time == 0) {
		/* FW didn't update the record (FW panic?) */
		SCSC_TAG_INFO(MXMAN, "FW failure updating the FW time\n");
		SCSC_TAG_INFO(MXMAN, "Sync delta %lld\n", ktime_to_ns(ktime_sub(t2, t1)));
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
		sync_r_mem->tv_sec = ts.tv_sec;
		sync_r_mem->tv_usec = ts.tv_nsec / NSEC_PER_USEC;
#else
		sync_r_mem->tv_sec = (u64)t.tv_sec;
		sync_r_mem->tv_usec = (u64)t.tv_usec;
#endif
		sync_r_mem->kernel_time = ktime_to_ns(t2);
		sync_r_mem->sync_event = event;
		return 0;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	sync_r_mem->tv_sec = ts.tv_sec;
	sync_r_mem->tv_usec = ts.tv_nsec / NSEC_PER_USEC;
#else
	sync_r_mem->tv_sec = (u64)t.tv_sec;
	sync_r_mem->tv_usec = (u64)t.tv_usec;
#endif
	sync_r_mem->kernel_time = ktime_to_ns(t2);
	sync_r_mem->sync_event = event;

	SCSC_TAG_INFO(MXMAN, "Sample, %lld, %u, %lld.%06lld\n",
		ktime_to_ns(sync_r_mem->kernel_time), sync_r_mem->fw_time, sync_r_mem->tv_sec, sync_r_mem->tv_usec);
	SCSC_TAG_INFO(MXMAN, "Sync delta %lld\n", ktime_to_ns(ktime_sub(t2, t1)));

	mxlogger->sync_buffer_index++;
	mxlogger->sync_buffer_index &= SYNC_MASK;

	return 0;
}

int mxlogger_generate_sync_record(struct mxlogger *mxlogger, enum mxlogger_sync_event event)
{
	int r;

	mutex_lock(&mxlogger->lock);
	r = __mxlogger_generate_sync_record(mxlogger, event);
	mutex_unlock(&mxlogger->lock);

	return r;
}

static void mxlogger_wait_for_msg_reinit_completion(struct mxlogger *mxlogger)
{
	reinit_completion(&mxlogger->rings_serialized_ops);
}

static bool mxlogger_wait_for_msg_reply(struct mxlogger *mxlogger)
{
	int ret;

	ret = wait_for_completion_timeout(&mxlogger->rings_serialized_ops, usecs_to_jiffies(MXLOGGER_RINGS_TMO_US));
	if (ret) {
		int i;

		SCSC_TAG_DBG3(MXMAN, "MXLOGGER RINGS -- replied in %lu usecs.\n",
			      MXLOGGER_RINGS_TMO_US - jiffies_to_usecs(ret));

		for (i = 0; i < MXLOGGER_NUM_BUFFERS; i++)
			SCSC_TAG_DBG3(MXMAN, "MXLOGGER:: RING[%d] -- INFO[0x%X]  STATUS[0x%X]\n", i,
				      mxlogger->cfg->bfds[i].info, mxlogger->cfg->bfds[i].status);
	} else {
		SCSC_TAG_ERR(MXMAN, "MXLOGGER timeout waiting for reply.\n");
	}

	return ret ? true : false;
}

static inline void __mxlogger_enable(struct mxlogger *mxlogger, bool enable, uint8_t reason)
{
	struct log_msg_packet msg = {};

	msg.msg = MM_MXLOGGER_LOGGER_CMD;
	msg.arg = (enable) ? MM_MXLOGGER_LOGGER_ENABLE : MM_MXLOGGER_LOGGER_DISABLE;
	msg.payload[0] = reason;

	/* Reinit the completion before sending the message over cpacketbuffer
	 * otherwise there might be a race condition
	 */
	mxlogger_wait_for_msg_reinit_completion(mxlogger);

	mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(mxlogger->mx),
			      MMTRANS_CHAN_ID_MAXWELL_LOGGING,
			      &msg, sizeof(msg));

	SCSC_TAG_DBG4(MXMAN, "MXLOGGER RINGS -- enable:%d  reason:%d\n",
		      enable, reason);

	mxlogger_wait_for_msg_reply(mxlogger);
}

static void mxlogger_enable(struct mxlogger *mxlogger, bool enable)
{
	return __mxlogger_enable(mxlogger, enable, MM_MXLOGGER_DISABLE_REASON_STOP);
}

static int mxlogger_send_config(struct mxlogger *mxlogger)
{
	struct log_msg_packet msg = {};

	SCSC_TAG_INFO(MXMAN, "MXLOGGER Config mifram_ref: 0x%x  size:%d\n",
		      mxlogger->mifram_ref, mxlogger->msz);

	msg.msg = MM_MXLOGGER_CONFIG_CMD;
	msg.arg = MM_MXLOGGER_CONFIG_BASE_ADDR;
	memcpy(&msg.payload, &mxlogger->mifram_ref, sizeof(mxlogger->mifram_ref));

	/* Reinit the completion before sending the message over cpacketbuffer
	 * otherwise there might be a race condition
	 */
	mxlogger_wait_for_msg_reinit_completion(mxlogger);

	mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(mxlogger->mx),
			      MMTRANS_CHAN_ID_MAXWELL_LOGGING,
			      &msg, sizeof(msg));

	SCSC_TAG_INFO(MXMAN, "MXLOGGER Config SENT\n");
	if (!mxlogger_wait_for_msg_reply(mxlogger))
		return -1;

	return 0;
}

static void mxlogger_to_shared_dram(struct mxlogger *mxlogger)
{
	int r;
	struct log_msg_packet msg = { .msg = MM_MXLOGGER_DIRECTION_CMD,
				      .arg = MM_MXLOGGER_DIRECTION_DRAM };

	SCSC_TAG_INFO(MXMAN, "MXLOGGER -- NO active observers detected. Send logs to DRAM\n");

	r = __mxlogger_generate_sync_record(mxlogger, MXLOGGER_SYN_TORAM);
	if (r)
		return;	/* mxlogger is not configured */

	mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(mxlogger->mx),
			      MMTRANS_CHAN_ID_MAXWELL_LOGGING,
			      &msg, sizeof(msg));
}

static void mxlogger_to_host(struct mxlogger *mxlogger)
{
	int r;
	struct log_msg_packet msg = { .msg = MM_MXLOGGER_DIRECTION_CMD,
				      .arg = MM_MXLOGGER_DIRECTION_HOST };

	SCSC_TAG_INFO(MXMAN, "MXLOGGER -- active observers detected. Send logs to host\n");

	r = __mxlogger_generate_sync_record(mxlogger, MXLOGGER_SYN_TOHOST);
	if (r)
		return; /* mxlogger is not configured */

	mxmgmt_transport_send(scsc_mx_get_mxmgmt_transport(mxlogger->mx),
			      MMTRANS_CHAN_ID_MAXWELL_LOGGING,
			      &msg, sizeof(msg));
}

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
static void mxlogger_disable_for_collection(struct mxlogger *mxlogger)
{
	return __mxlogger_enable(mxlogger, false, MM_MXLOGGER_DISABLE_REASON_COLLECTION);
}

static int mxlogger_collect_init(struct scsc_log_collector_client *collect_client)
{
	struct mxlogger *mxlogger = (struct mxlogger *)collect_client->prv;

	if (!mxlogger->initialized)
		return 0;

	mutex_lock(&mxlogger->lock);

	SCSC_TAG_INFO(MXMAN, "Started log collection\n");

	__mxlogger_generate_sync_record(mxlogger, MXLOGGER_SYN_LOGCOLLECTION);

	mxlogger->re_enable = mxlogger->enabled;
	/**
	 * If enabled, tell FW we are stopping for collection:
	 * this way FW can dump last minute stuff and flush properly
	 * its cache
	 */
	if (mxlogger->enabled)
		mxlogger_disable_for_collection(mxlogger);

	mutex_unlock(&mxlogger->lock);

	return 0;
}

static int mxlogger_collect(struct scsc_log_collector_client *collect_client, size_t size)
{
	struct scsc_mif_abs *mif;
	struct mxlogger *mxlogger = (struct mxlogger *)collect_client->prv;
	void *buf;
	int ret = 0;
	int i;
	size_t sz;

	if (mxlogger && mxlogger->mx)
		mif = scsc_mx_get_mif_abs(mxlogger->mx);
	else
		/* Return 0 as 'success' to continue the collection of other chunks */
		return 0;

	mutex_lock(&mxlogger->lock);

	if (mxlogger->initialized == false) {
		SCSC_TAG_ERR(MXMAN, "MXLOGGER not initialized\n");
		mutex_unlock(&mxlogger->lock);
		return 0;
	}

	if (collect_client->type == SCSC_LOG_CHUNK_SYNC)
		i = MXLOGGER_SYNC;
	else if (collect_client->type == SCSC_LOG_CHUNK_IMP)
		i = MXLOGGER_IMP;
	else if (collect_client->type == SCSC_LOG_RESERVED_COMMON)
		i = MXLOGGER_RESERVED_COMMON;
	else if (collect_client->type == SCSC_LOG_RESERVED_BT)
		i = MXLOGGER_RESERVED_BT;
	else if (collect_client->type == SCSC_LOG_RESERVED_WLAN)
		i = MXLOGGER_RESERVED_WLAN;
	else if (collect_client->type == SCSC_LOG_RESERVED_RADIO)
		i = MXLOGGER_RESERVED_RADIO;
	else if (collect_client->type == SCSC_LOG_CHUNK_MXL)
		i = MXLOGGER_MXLOG;
	else if (collect_client->type == SCSC_LOG_CHUNK_UDI)
		i = MXLOGGER_UDI;
	else {
		SCSC_TAG_ERR(MXMAN, "MXLOGGER Incorrect type. Return 'success' and continue to collect other buffers\n");
		mutex_unlock(&mxlogger->lock);
		return 0;
	}

	sz = mxlogger->cfg->bfds[i].size;
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	buf = mif->get_mifram_ptr_region2(mif, mxlogger->cfg->bfds[i].location);
#else
	buf = mif->get_mifram_ptr(mif, mxlogger->cfg->bfds[i].location);
#endif
	SCSC_TAG_INFO(MXMAN, "Writing buffer %s size: %zu\n", mxlogger_buf_name[i], sz);
	ret = scsc_log_collector_write(buf, sz, 1);
	if (ret) {
		mutex_unlock(&mxlogger->lock);
		return ret;
	}

	mutex_unlock(&mxlogger->lock);
	return 0;
}

static int mxlogger_collect_end(struct scsc_log_collector_client *collect_client)
{
	struct mxlogger *mxlogger = (struct mxlogger *)collect_client->prv;

	if (!mxlogger->initialized)
		return 0;

	mutex_lock(&mxlogger->lock);

	SCSC_TAG_INFO(MXMAN, "End log collection\n");

	/* Renable again if was previoulsy enabled */
	if (mxlogger->re_enable)
		mxlogger_enable(mxlogger, true);

	mutex_unlock(&mxlogger->lock);
	return 0;
}
#endif

void mxlogger_print_mapping(struct mxlogger_config_area *cfg)
{
	u8 i;

	SCSC_TAG_INFO(MXMAN, "MXLOGGER  -- Configured Buffers [%d]\n", cfg->config.num_buffers);
	for (i = 0; i < MXLOGGER_NUM_BUFFERS; i++)
		SCSC_TAG_INFO(MXMAN, "buffer %s loc: 0x%08x size: %u\n",
			      mxlogger_buf_name[i], cfg->bfds[i].location, cfg->bfds[i].size);

}

/* Lock should be acquired by caller */
int mxlogger_init(struct scsc_mx *mx, struct mxlogger *mxlogger, uint32_t mem_sz)
{
	struct miframman *miframman;
	struct scsc_mif_abs *mif = scsc_mx_get_mif_abs(mx);
	struct mxlogger_config_area *cfg;
	size_t remaining_mem;
	size_t udi_mxl_mem_sz;
	struct mxlogger_node *mn;
	uint32_t manual_total;

	MEM_LAYOUT_CHECK();

	mxlogger->configured = false;

	if (!mxlogger_manual_layout) {
		if (mem_sz <= (sizeof(struct mxlogger_config_area) + MXLOGGER_TOTAL_FIX_BUF)) {
			SCSC_TAG_ERR(MXMAN, "Insufficient memory allocation\n");
			return -EIO;
		}
	} else {
		manual_total = mxlogger_manual_imp + mxlogger_manual_rsv_common +
			       mxlogger_manual_rsv_bt + mxlogger_manual_rsv_wlan +
			       mxlogger_manual_rsv_radio + mxlogger_manual_mxlog +
			       mxlogger_manual_udi;

		SCSC_TAG_INFO(MXMAN, "MXLOGGER Manual layout requested %d of total %d\n", manual_total, mxlogger_manual_total_mem);
		if (manual_total > mxlogger_manual_total_mem)  {
			SCSC_TAG_ERR(MXMAN, "Insufficient memory allocation for FW_layout\n");
			return -EIO;
		}
	}

	mxlogger->mx = mx;
	miframman = scsc_mx_get_ramman2(mx);
	if (!miframman)
		return -ENOMEM;
	mxlogger->mem = miframman_alloc(miframman, mem_sz, 32, MIFRAMMAN_OWNER_COMMON);
	if (!mxlogger->mem) {
		SCSC_TAG_ERR(MXMAN, "Error allocating memory for MXLOGGER\n");
		return -ENOMEM;
	}
	mxlogger->msz = mem_sz;

	/* Clear memory to avoid reading old records */
	memset(mxlogger->mem, 0, mxlogger->msz);
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	mif->get_mifram_ref_region2(mif, mxlogger->mem, &mxlogger->mifram_ref);
#else
	mif->get_mifram_ref(mif, mxlogger->mem, &mxlogger->mifram_ref);
#endif

	mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport(mxlogger->mx),
						  MMTRANS_CHAN_ID_MAXWELL_LOGGING,
						  &mxlogger_message_handler, mxlogger);

	/* Initialize configuration structure */
	SCSC_TAG_INFO(MXMAN, "MXLOGGER Configuration: 0x%x\n", (u32)mxlogger->mifram_ref);
	cfg = (struct mxlogger_config_area *)mxlogger->mem;

	cfg->config.magic_number = MXLOGGER_MAGIG_NUMBER;
	cfg->config.config_major = MXLOGGER_MAJOR;
	cfg->config.config_minor = MXLOGGER_MINOR;
	cfg->config.num_buffers = MXLOGGER_NUM_BUFFERS;

	/**
	 * Populate information of Fixed size buffers
	 * These are mifram-reletive references
	 */
	cfg->bfds[MXLOGGER_SYNC].location = mxlogger->mifram_ref +
		offsetof(struct mxlogger_config_area, buffers_start);
	cfg->bfds[MXLOGGER_SYNC].size = MXLOGGER_SYNC_SIZE;
	/* additionally cache the va of sync_buffer */
	mxlogger->mem_sync_buf = mxlogger->mem +
		offsetof(struct mxlogger_config_area, buffers_start);

	cfg->bfds[MXLOGGER_IMP].location =
		cfg->bfds[MXLOGGER_IMP - 1].location +
		cfg->bfds[MXLOGGER_IMP - 1].size;
	cfg->bfds[MXLOGGER_IMP].size =
		mxlogger_manual_layout ? mxlogger_manual_imp : MXLOGGER_IMP_SIZE;

	cfg->bfds[MXLOGGER_RESERVED_COMMON].location =
		cfg->bfds[MXLOGGER_RESERVED_COMMON - 1].location +
		cfg->bfds[MXLOGGER_RESERVED_COMMON - 1].size;
	cfg->bfds[MXLOGGER_RESERVED_COMMON].size =
		mxlogger_manual_layout ? mxlogger_manual_rsv_common : MXLOGGER_RSV_COMMON_SZ;

	cfg->bfds[MXLOGGER_RESERVED_BT].location =
		cfg->bfds[MXLOGGER_RESERVED_BT - 1].location +
		cfg->bfds[MXLOGGER_RESERVED_BT - 1].size;
	cfg->bfds[MXLOGGER_RESERVED_BT].size =
		mxlogger_manual_layout ? mxlogger_manual_rsv_bt : MXLOGGER_RSV_BT_SZ;

	cfg->bfds[MXLOGGER_RESERVED_WLAN].location =
		cfg->bfds[MXLOGGER_RESERVED_WLAN - 1].location +
		cfg->bfds[MXLOGGER_RESERVED_WLAN - 1].size;
	cfg->bfds[MXLOGGER_RESERVED_WLAN].size =
		mxlogger_manual_layout ? mxlogger_manual_rsv_wlan : MXLOGGER_RSV_WLAN_SZ;

	cfg->bfds[MXLOGGER_RESERVED_RADIO].location =
		cfg->bfds[MXLOGGER_RESERVED_RADIO - 1].location +
		cfg->bfds[MXLOGGER_RESERVED_RADIO - 1].size;
	cfg->bfds[MXLOGGER_RESERVED_RADIO].size =
		mxlogger_manual_layout ? mxlogger_manual_rsv_radio : MXLOGGER_RSV_RADIO_SZ;

	/* Compute buffer locations and size based on the remaining space */
	remaining_mem = mem_sz - (sizeof(struct mxlogger_config_area) + MXLOGGER_TOTAL_FIX_BUF);

	/* Align the buffer to be cache friendly */
	udi_mxl_mem_sz = (remaining_mem >> 1) & ~(MXLOGGER_NON_FIX_BUF_ALIGN - 1);

	SCSC_TAG_INFO(MXMAN, "remaining_mem %zu udi/mxlogger size %zu\n", remaining_mem, udi_mxl_mem_sz);

	cfg->bfds[MXLOGGER_MXLOG].location =
		cfg->bfds[MXLOGGER_MXLOG - 1].location +
		cfg->bfds[MXLOGGER_MXLOG - 1].size;
	cfg->bfds[MXLOGGER_MXLOG].size =
		mxlogger_manual_layout ? mxlogger_manual_mxlog : udi_mxl_mem_sz;

	cfg->bfds[MXLOGGER_UDI].location =
		cfg->bfds[MXLOGGER_UDI - 1].location +
		cfg->bfds[MXLOGGER_UDI - 1].size;
	cfg->bfds[MXLOGGER_UDI].size =
		mxlogger_manual_layout ? mxlogger_manual_udi : udi_mxl_mem_sz;

	/* Save offset to buffers array */
#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
	mif->get_mifram_ref_region2(mif, cfg->bfds, &cfg->config.bfds_ref);
#else
	mif->get_mifram_ref(mif, cfg->bfds, &cfg->config.bfds_ref);
#endif

	mxlogger_print_mapping(cfg);

	mxlogger->cfg = cfg;

	init_completion(&mxlogger->rings_serialized_ops);
	mxlogger->enabled = false;

	mutex_init(&mxlogger->lock);

	mn = kzalloc(sizeof(*mn), GFP_KERNEL);
	if (!mn) {
		miframman_free(miframman, mxlogger->mem);
		return -ENOMEM;
	}

	/**
	 * Update observers status considering
	 * current value of mxlogger_forced_to_host
	 */
	update_fake_observer();

	mutex_lock(&global_lock);
	mxlogger->observers = active_global_observers;
	if (mxlogger->observers)
		SCSC_TAG_INFO(MXMAN, "Detected global %d observer[s]\n", active_global_observers);
	mutex_unlock(&global_lock);

	mxlogger->sync_buffer_index = 0;

	mn->mxl = mxlogger;
	list_add_tail(&mn->list, &mxlogger_list.list);

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	/**
	 * Register to the collection infrastructure
	 *
	 * All of mxlogger buffers are registered here, NO matter if
	 * MXLOGGER initialization was successfull FW side.
	 *
	 * In such a case MXLOGGER-FW will simply ignore all of our following
	 * requests and we'll end up dumping empty buffers, BUT with a partially
	 * meaningful sync buffer. (since this last is written also Host side)
	 */
	mxlogger_collect_client_sync.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_sync);

	mxlogger_collect_client_imp.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_imp);

	mxlogger_collect_client_rsv_common.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_rsv_common);

	mxlogger_collect_client_rsv_bt.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_rsv_bt);

	mxlogger_collect_client_rsv_wlan.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_rsv_wlan);

	mxlogger_collect_client_rsv_radio.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_rsv_radio);

	mxlogger_collect_client_udi.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_udi);

	mxlogger_collect_client_mxl.prv = mxlogger;
	scsc_log_collector_register_client(&mxlogger_collect_client_mxl);
#endif
	mxlogger->configured = true;
	SCSC_TAG_INFO(MXMAN, "MXLOGGER Configured\n");
	return 0;
}

int mxlogger_start(struct mxlogger *mxlogger)
{
	if (mxlogger_disabled) {
		SCSC_TAG_WARNING(MXMAN, "MXLOGGER is disabled. Not Starting.\n");
		return -1;
	}

	if (!mxlogger || !mxlogger->configured) {
		SCSC_TAG_WARNING(MXMAN, "MXLOGGER is not valid or not configured.\n");
		return -1;
	}

	SCSC_TAG_INFO(MXMAN, "Starting mxlogger with %d observer[s]\n", mxlogger->observers);

	mutex_lock(&mxlogger->lock);
	if (mxlogger_send_config(mxlogger)) {
		mutex_unlock(&mxlogger->lock);
		return -ENOMEM;
	}

	/**
	 * MXLOGGER on FW-side is at this point starting up too during
	 * WLBT chip boot and it cannot make any assumption till about
	 * the current number of observers and direction set: so, during
	 * MXLOGGER FW-side initialization, ZERO observers were registered.
	 *
	 * As a consequence on chip-boot FW-MXLOGGER defaults to:
	 *  - direction DRAM
	 *  - all rings disabled (ingressing messages discarded)
	 */
	if (!mxlogger->observers) {
		/* Enabling BEFORE communicating direction DRAM
		 * to avoid losing messages on rings.
		 */
		mxlogger_enable(mxlogger, true);
		mxlogger_to_shared_dram(mxlogger);
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
		scsc_log_collector_is_observer(false);
#endif
	} else {
		mxlogger_to_host(mxlogger);
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
		scsc_log_collector_is_observer(true);
#endif
		/* Enabling AFTER communicating direction HOST
		 * to avoid wrongly spilling messages into the
		 * rings early at start (like at boot).
		 */
		mxlogger_enable(mxlogger, true);
	}

	SCSC_TAG_INFO(MXMAN, "MXLOGGER Started.\n");
	mutex_unlock(&mxlogger->lock);

	return 0;
}

void mxlogger_deinit(struct scsc_mx *mx, struct mxlogger *mxlogger)
{
	struct miframman *miframman = NULL;
	struct mxlogger_node *mn, *next;
	bool match = false;

	SCSC_TAG_INFO(MXMAN, "\n");

	if (!mxlogger || !mxlogger->configured) {
		SCSC_TAG_WARNING(MXMAN, "MXLOGGER is not valid or not configured.\n");
		return;
	}
	/* Run deregistration before adquiring the mxlogger lock to avoid
	 * deadlock with log_collector.
	 */
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	scsc_log_collector_unregister_client(&mxlogger_collect_client_sync);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_imp);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_rsv_common);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_rsv_bt);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_rsv_wlan);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_rsv_radio);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_mxl);
	scsc_log_collector_unregister_client(&mxlogger_collect_client_udi);
#endif
	mutex_lock(&mxlogger->lock);

	mxlogger_to_host(mxlogger);	/* immediately before deconfigure to get a last sync rec */
	mxlogger->configured = false;
	mxlogger->initialized = false;

#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	scsc_log_collector_is_observer(true);
#endif
	mxlogger_enable(mxlogger, false);
	mxmgmt_transport_register_channel_handler(scsc_mx_get_mxmgmt_transport(mxlogger->mx),
						  MMTRANS_CHAN_ID_MAXWELL_LOGGING,
						  NULL, NULL);
	miframman = scsc_mx_get_ramman2(mx);
	if (miframman)
		miframman_free(miframman, mxlogger->mem);

	list_for_each_entry_safe(mn, next, &mxlogger_list.list, list) {
		if (mn->mxl == mxlogger) {
			match = true;
			list_del(&mn->list);
			kfree(mn);
		}
	}

	if (match == false)
		SCSC_TAG_ERR(MXMAN, "FATAL, no match for given scsc_mif_abs\n");

	SCSC_TAG_INFO(MXMAN, "End\n");
	mutex_unlock(&mxlogger->lock);
}

int mxlogger_register_observer(struct mxlogger *mxlogger, char *name)
{
	mutex_lock(&mxlogger->lock);

	mxlogger->observers++;

	SCSC_TAG_INFO(MXMAN, "Register observer[%d] -- %s\n",
		      mxlogger->observers, name);

	/* Switch logs to host */
	mxlogger_to_host(mxlogger);
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	scsc_log_collector_is_observer(true);
#endif

	mutex_unlock(&mxlogger->lock);

	return 0;
}

int mxlogger_unregister_observer(struct mxlogger *mxlogger, char *name)
{
	mutex_lock(&mxlogger->lock);

	if (mxlogger->observers == 0) {
		SCSC_TAG_INFO(MXMAN, "Incorrect number of observers\n");
		mutex_unlock(&mxlogger->lock);
		return -EIO;
	}

	mxlogger->observers--;

	SCSC_TAG_INFO(MXMAN, "UN-register observer[%d] --  %s\n",
		      mxlogger->observers, name);

	if (mxlogger->observers == 0) {
		mxlogger_to_shared_dram(mxlogger);
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
		scsc_log_collector_is_observer(false);
#endif
	}

	mutex_unlock(&mxlogger->lock);

	return 0;
}

/* Global observer are not associated to any [mx] mxlogger instance. So it registers as
 * an observer to all the [mx] mxlogger instances.
 */
int mxlogger_register_global_observer(char *name)
{
	struct mxlogger_node *mn, *next;

	mutex_lock(&global_lock);
	active_global_observers++;

	SCSC_TAG_INFO(MXMAN, "Register global observer[%d] -- %s\n",
		      active_global_observers, name);

	if (list_empty(&mxlogger_list.list)) {
		SCSC_TAG_INFO(MXMAN, "No instances of mxman\n");
		mutex_unlock(&global_lock);
		return -EIO;
	}

	list_for_each_entry_safe(mn, next, &mxlogger_list.list, list) {
		/* There is a mxlogger instance */
		mxlogger_register_observer(mn->mxl, name);
	}

	mutex_unlock(&global_lock);

	return 0;
}

int mxlogger_unregister_global_observer(char *name)
{
	struct mxlogger_node *mn, *next;

	mutex_lock(&global_lock);
	if (active_global_observers)
		active_global_observers--;

	SCSC_TAG_INFO(MXMAN, "UN-register global observer[%d] --  %s\n",
		      active_global_observers, name);

	list_for_each_entry_safe(mn, next, &mxlogger_list.list, list) {
		/* There is a mxlogger instance */
		mxlogger_unregister_observer(mn->mxl, name);
	}

	mutex_unlock(&global_lock);

	return 0;
}

