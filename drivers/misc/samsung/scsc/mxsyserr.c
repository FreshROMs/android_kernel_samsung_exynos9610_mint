/****************************************************************************
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/kmod.h>
#include <linux/notifier.h>

#include "scsc_mx_impl.h"
#include "miframman.h"
#include "mifmboxman.h"
#include "mxman.h"
#include "srvman.h"
#include "mxmgmt_transport.h"
#include "mxlog.h"
#include "mxlogger.h"
#include "fw_panic_record.h"
#include "panicmon.h"
#include "mxproc.h"
#include "mxsyserr.h"
#include "scsc/scsc_log_collector.h"

#include <scsc/scsc_release.h>
#include <scsc/scsc_mx.h>
#include <scsc/scsc_logring.h>

/* If limits below are exceeded, a service level reset will be raised to level 7 */
#define SYSERR_RESET_HISTORY_SIZE      (4)
/* Minimum time between system error service resets (ms) */
#define SYSERR_RESET_MIN_INTERVAL      (300000)
/* No more then SYSERR_RESET_HISTORY_SIZE system error service resets in this period (ms)*/
#define SYSERR_RESET_MONITOR_PERIOD    (3600000)


/* Time stamps of last service resets in jiffies */
static unsigned long syserr_reset_history[SYSERR_RESET_HISTORY_SIZE] = {0};
static int syserr_reset_history_index;

static uint syserr_reset_min_interval = SYSERR_RESET_MIN_INTERVAL;
module_param(syserr_reset_min_interval, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(syserr_reset_min_interval, "Minimum time between system error service resets (ms)");

static uint syserr_reset_monitor_period = SYSERR_RESET_MONITOR_PERIOD;
module_param(syserr_reset_monitor_period, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(syserr_reset_monitor_period, "No more then 4 system error service resets in this period (ms)");


void mx_syserr_init(void)
{
	SCSC_TAG_INFO(MXMAN, "MM_SYSERR_INIT: syserr_reset_min_interval %lu syserr_reset_monitor_period %lu\n",
		syserr_reset_min_interval, syserr_reset_monitor_period);
}

void mx_syserr_handler(struct mxman *mxman, const void *message)
{
	const struct mx_syserr_msg *msg = (const struct mx_syserr_msg *)message;
	struct srvman *srvman;

	struct mx_syserr_decode	decode;

	srvman = scsc_mx_get_srvman(mxman->mx);

	SCSC_TAG_INFO(MXMAN, "MM_SYSERR_IND len: %u, ts: 0x%08X, tf: 0x%08X, str: 0x%x, code: 0x%08x, p0: 0x%x, p1: 0x%x\n",
		msg->syserr.length,
		msg->syserr.slow_clock,
		msg->syserr.fast_clock,
		msg->syserr.string_index,
		msg->syserr.syserr_code,
		msg->syserr.param[0],
		msg->syserr.param[1]);

	decode.subsys = (u8) ((msg->syserr.syserr_code >> SYSERR_SUB_SYSTEM_POSN) & SYSERR_SUB_SYSTEM_MASK);
	decode.level = (u8) ((msg->syserr.syserr_code >> SYSERR_LEVEL_POSN) & SYSERR_LEVEL_MASK);
	decode.type = (u8) ((msg->syserr.syserr_code >> SYSERR_TYPE_POSN) & SYSERR_TYPE_MASK);
	decode.subcode = (u16) ((msg->syserr.syserr_code >> SYSERR_SUB_CODE_POSN) & SYSERR_SUB_CODE_MASK);

	SCSC_TAG_INFO(MXMAN, "MM_SYSERR_IND Subsys %d, Level %d, Type %d, Subcode 0x%04x\n",
				decode.subsys, decode.level, decode.type, decode.subcode);

	/* Level 1 just gets logged without bothering anyone else */
	if (decode.level == MX_SYSERR_LEVEL_1) {
		SCSC_TAG_INFO(MXMAN, "MM_SYSERR_IND code: 0x%08x log only\n",
			msg->syserr.syserr_code);
		return;
	}

	/* Ignore if panic reset in progress */
	if (srvman_in_error_safe(srvman) || (mxman->mxman_state == MXMAN_STATE_FAILED)) {
		SCSC_TAG_INFO(MXMAN, "MM_SYSERR_IND code: 0x%08x ignored (reset in progess)\n",
			msg->syserr.syserr_code);
		return;
	}

	/* Ignore any system errors for the same sub-system if recovery is in progress */
	if ((mxman->syserr_recovery_in_progress) && (mxman->last_syserr.subsys == decode.subsys)) {
		SCSC_TAG_INFO(MXMAN, "MM_SYSERR_IND code: 0x%08x ignored (recovery in progess)\n",
			msg->syserr.syserr_code);
		return;
	}


	/* Let affected sevices escalate if needed - this also checks if only one sub-system is running
	 * and handles race conditions with shutting down service
	 */
	decode.level = srvman_notify_sub_system(srvman, &decode);

	if (decode.level >= MX_SYSERR_LEVEL_5) {
		unsigned long now = jiffies;
		int i;

		/* We use 0 as a NULL timestamp so avoid this */
		now = (now) ? now : 1;

		if ((decode.level >= MX_SYSERR_LEVEL_7) || (mxman->syserr_recovery_in_progress)) {
			/* If full reset has been requested or a service restart is needed and one is
			 * already in progress, trigger a full reset
			 */
			SCSC_TAG_INFO(MXMAN, "MM_SYSERR_IND code: 0x%08x triggered full reset\n",
				msg->syserr.syserr_code);

			mxman_fail(mxman, SCSC_PANIC_CODE_HOST << 15, __func__);
			return;
		}

		/* last_syserr_recovery_time is always zero-ed before we restart the chip */
		if (mxman->last_syserr_recovery_time) {
			/* Have we had a too recent system error service reset
			 * Chance of false positive here is low enough to be acceptable
			 */
			if ((syserr_reset_min_interval) && (time_in_range(now, mxman->last_syserr_recovery_time,
					mxman->last_syserr_recovery_time + msecs_to_jiffies(syserr_reset_min_interval)))) {

				SCSC_TAG_INFO(MXMAN, "MM_SYSERR_IND code: 0x%08x triggered full reset (less than %dms after last)\n",
					msg->syserr.syserr_code, syserr_reset_min_interval);
				mxman_fail(mxman, SCSC_PANIC_CODE_HOST << 15, __func__);
				return;
			} else if (syserr_reset_monitor_period) {
				/* Have we had too many system error service resets in one period? */
				/* This will be the case if all our stored history was in this period */
				bool out_of_danger_period_found = false;

				for (i = 0; (i < SYSERR_RESET_HISTORY_SIZE) && (!out_of_danger_period_found); i++)
					out_of_danger_period_found = ((!syserr_reset_history[i]) ||
							      (!time_in_range(now, syserr_reset_history[i],
								syserr_reset_history[i] + msecs_to_jiffies(syserr_reset_monitor_period))));

				if (!out_of_danger_period_found) {
					SCSC_TAG_INFO(MXMAN, "MM_SYSERR_IND code: 0x%08x triggered full reset (too many within %dms)\n",
						msg->syserr.syserr_code, syserr_reset_monitor_period);
					mxman_fail(mxman, SCSC_PANIC_CODE_HOST << 15, __func__);
					return;
				}
			}
		} else
			/* First syserr service reset since chip was (re)started - zap history */
			for (i = 0; i < SYSERR_RESET_HISTORY_SIZE; i++)
				syserr_reset_history[i] = 0;

		/* Otherwise trigger recovery of the affected subservices */
		SCSC_TAG_INFO(MXMAN, "MM_SYSERR_IND code: 0x%08x triggered service recovery\n",
			msg->syserr.syserr_code);
		syserr_reset_history[syserr_reset_history_index++ % SYSERR_RESET_HISTORY_SIZE] = now;
		mxman->last_syserr_recovery_time = now;
		mxman_syserr(mxman, &decode);
	}

#ifdef CONFIG_SCSC_WLBTD
#if IS_ENABLED(CONFIG_SCSC_LOG_COLLECTION)
	/* Trigger sable log collection */
	SCSC_TAG_INFO(MXMAN, "MM_SYSERR_IND code: 0x%08x requested log collection\n", msg->syserr.syserr_code);
	scsc_log_collector_schedule_collection(SCSC_LOG_SYS_ERR, decode.subcode);
#endif
#endif
}
