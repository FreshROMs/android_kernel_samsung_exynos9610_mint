/****************************************************************************
 *
 * Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/module.h>
#include <linux/slab.h>
#include <scsc/scsc_logring.h>
#include "scsc_mx_impl.h"
#include "mifintrbit.h"
#include "miframman.h"
#include "mifmboxman.h"
#ifdef CONFIG_SCSC_SMAPPER
#include "mifsmapper.h"
#endif
#ifdef CONFIG_SCSC_QOS
#include "mifqos.h"
#endif
#include "mifproc.h"
#include "mxman.h"
#include "mxproc.h"
#include "mxsyserr.h"
#include "srvman.h"
#include "mxmgmt_transport.h"
#include "gdb_transport.h"
#include "mxlog.h"
#include "mxlogger.h"
#include "panicmon.h"
#include "mxlog_transport.h"
#include "suspendmon.h"

#include "scsc/api/bt_audio.h"
#include "mxfwconfig.h"
#ifdef CONFIG_SCSC_WLBTD
#include "scsc_wlbtd.h"
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <scsc/scsc_wakelock.h>
#else
#include <linux/wakelock.h>
#endif

struct scsc_mx {
	struct scsc_mif_abs     *mif_abs;
	struct mifintrbit       intr;
	struct miframman        ram;
	struct miframman        ram2;
	struct mifmboxman       mbox;
	struct mifabox          mifabox;
#ifdef CONFIG_SCSC_SMAPPER
	struct mifsmapper	smapper;
#endif
#ifdef CONFIG_SCSC_QOS
	struct mifqos		qos;
#endif
	struct mifproc          proc;
	struct mxman            mxman;
	struct srvman           srvman;
	struct mxmgmt_transport mxmgmt_transport;
	struct gdb_transport    gdb_transport_r4;
	struct gdb_transport    gdb_transport_m4;
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	struct gdb_transport    gdb_transport_m4_1;
#endif
	int                     users;
	struct mxlog            mxlog;
	struct mxlogger         mxlogger;
	struct panicmon         panicmon;
	struct mxlog_transport  mxlog_transport;
	struct suspendmon	suspendmon;
	struct mxfwconfig	mxfwconfig;
	struct mutex            scsc_mx_read_mutex;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	struct scsc_wake_lock   scsc_mx_wl_request_firmware;
#else
	struct wake_lock        scsc_mx_wl_request_firmware;
#endif
};


struct scsc_mx *scsc_mx_create(struct scsc_mif_abs *mif)
{
	struct scsc_mx *mx;

	mx = kzalloc(sizeof(*mx), GFP_KERNEL);
	if (!mx)
		return NULL;

	mx->mif_abs = mif;

	mifintrbit_init(&mx->intr, mif);

	mifmboxman_init(&mx->mbox);
	suspendmon_init(&mx->suspendmon, mx);
	mxman_init(&mx->mxman, mx);
	srvman_init(&mx->srvman, mx);
	mifproc_create_proc_dir(mx->mif_abs);
#ifdef CONFIG_SCSC_WLBTD
	scsc_wlbtd_init();
#endif
	mx_syserr_init();
	mutex_init(&mx->scsc_mx_read_mutex);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	wake_lock_init(&mx->scsc_mx_wl_request_firmware, WAKE_LOCK_SUSPEND, "scsc_mx_request_firmware");
#else
	wake_lock_init(NULL, &mx->scsc_mx_wl_request_firmware.ws, "scsc_mx_request_firmware");
#endif
	SCSC_TAG_DEBUG(MXMAN, "Hurray Maxwell is here with %p\n", mx);
	return mx;
}

void scsc_mx_destroy(struct scsc_mx *mx)
{
	SCSC_TAG_DEBUG(MXMAN, "\n");
	BUG_ON(mx == NULL);
	mifintrbit_deinit(&mx->intr);
	mifmboxman_deinit(scsc_mx_get_mboxman(mx));
#ifdef CONFIG_SCSC_SMAPPER
	mifsmapper_deinit(scsc_mx_get_smapper(mx));
#endif
	suspendmon_deinit(scsc_mx_get_suspendmon(mx));
	mifproc_remove_proc_dir();
	srvman_deinit(&mx->srvman);
	mxman_deinit(&mx->mxman);
	wake_lock_destroy(&mx->scsc_mx_wl_request_firmware);
	mutex_destroy(&mx->scsc_mx_read_mutex);
#ifdef CONFIG_SCSC_WLBTD
	scsc_wlbtd_deinit();
#endif
	kfree(mx);
	SCSC_TAG_DEBUG(MXMAN, "OK\n");
}

struct scsc_mif_abs *scsc_mx_get_mif_abs(struct scsc_mx *mx)
{
	return mx->mif_abs;
}

struct mifintrbit *scsc_mx_get_intrbit(struct scsc_mx *mx)
{
	return &mx->intr;
}

struct miframman *scsc_mx_get_ramman(struct scsc_mx *mx)
{
	return &mx->ram;
}

struct miframman *scsc_mx_get_ramman2(struct scsc_mx *mx)
{
	return &mx->ram2;
}

struct mifabox *scsc_mx_get_aboxram(struct scsc_mx *mx)
{
	return &mx->mifabox;
}

struct mifmboxman *scsc_mx_get_mboxman(struct scsc_mx *mx)
{
	return &mx->mbox;
}

#ifdef CONFIG_SCSC_SMAPPER
struct mifsmapper *scsc_mx_get_smapper(struct scsc_mx *mx)
{
	return &mx->smapper;
}
#endif

#ifdef CONFIG_SCSC_QOS
struct mifqos *scsc_mx_get_qos(struct scsc_mx *mx)
{
	return &mx->qos;
}
#endif

struct device *scsc_mx_get_device(struct scsc_mx *mx)
{
	return mx->mif_abs->get_mif_device(mx->mif_abs);
}
EXPORT_SYMBOL_GPL(scsc_mx_get_device); /* TODO: export a top-level API for this */

struct mxman *scsc_mx_get_mxman(struct scsc_mx *mx)
{
	return &mx->mxman;
}

struct srvman *scsc_mx_get_srvman(struct scsc_mx *mx)
{
	return &mx->srvman;
}

struct mxmgmt_transport *scsc_mx_get_mxmgmt_transport(struct scsc_mx *mx)
{
	return &mx->mxmgmt_transport;
}

struct gdb_transport *scsc_mx_get_gdb_transport_r4(struct scsc_mx *mx)
{
	return &mx->gdb_transport_r4;
}

struct gdb_transport *scsc_mx_get_gdb_transport_m4(struct scsc_mx *mx)
{
	return &mx->gdb_transport_m4;
}

#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
struct gdb_transport *scsc_mx_get_gdb_transport_m4_1(struct scsc_mx *mx)
{
	return &mx->gdb_transport_m4_1;
}
#endif

struct mxlog *scsc_mx_get_mxlog(struct scsc_mx *mx)
{
	return &mx->mxlog;
}

struct panicmon *scsc_mx_get_panicmon(struct scsc_mx *mx)
{
	return &mx->panicmon;
}

struct mxlog_transport *scsc_mx_get_mxlog_transport(struct scsc_mx *mx)
{
	return &mx->mxlog_transport;
}

struct mxlogger *scsc_mx_get_mxlogger(struct scsc_mx *mx)
{
	return &mx->mxlogger;
}

struct suspendmon *scsc_mx_get_suspendmon(struct scsc_mx *mx)
{
	return &mx->suspendmon;
}

struct mxfwconfig *scsc_mx_get_mxfwconfig(struct scsc_mx *mx)
{
	return &mx->mxfwconfig;
}

void scsc_mx_request_firmware_mutex_lock(struct scsc_mx *mx)
{
	mutex_lock(&mx->scsc_mx_read_mutex);
}
void scsc_mx_request_firmware_wake_lock(struct scsc_mx *mx)
{
	wake_lock(&mx->scsc_mx_wl_request_firmware);
}
void scsc_mx_request_firmware_mutex_unlock(struct scsc_mx *mx)
{
	mutex_unlock(&mx->scsc_mx_read_mutex);
}
void scsc_mx_request_firmware_wake_unlock(struct scsc_mx *mx)
{
	wake_unlock(&mx->scsc_mx_wl_request_firmware);
}
