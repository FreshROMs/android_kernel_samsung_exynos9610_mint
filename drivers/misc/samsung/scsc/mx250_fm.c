/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include <scsc/scsc_logring.h>
#include <scsc/scsc_mx.h>

#include "mxman.h"	/* Special case service driver that looks inside mxman */

#ifdef CONFIG_SCSC_FM_TEST
#include "mx250_fm_test.h"
#endif


struct scsc_mx_fm_client {
	/* scsc_service_client has to be the first */
	struct scsc_service_client fm_service_client;
	struct scsc_service	*fm_service;
	struct scsc_mx		*mx;
	bool			fm_api_available;
	scsc_mifram_ref		ref;
	struct workqueue_struct *fm_client_wq;
	struct work_struct	fm_client_work;
	struct completion	fm_client_work_completion;
	int			fm_client_work_completion_status;
	bool			ldo_on;
};

static struct scsc_mx_fm_client *fm_client;
/* service to start */
static int service_id = SCSC_SERVICE_ID_FM;

static DEFINE_MUTEX(ss_lock);

static u8 fm_client_failure_notification(struct scsc_service_client *client, struct mx_syserr_decode *err)
{
	(void)client;
	SCSC_TAG_DEBUG(FM, "OK\n");
	return err->level;
}


static bool fm_client_stop_on_failure(struct scsc_service_client *client, struct mx_syserr_decode *err)
{
	(void)client;
	(void)err;
	mutex_lock(&ss_lock);
	fm_client->fm_api_available = false;
	mutex_unlock(&ss_lock);
	SCSC_TAG_DEBUG(FM, "OK\n");
	return false;
}

static void fm_client_failure_reset(struct scsc_service_client *client, u8 level, u16 scsc_syserr_code)
{
	(void)client;
	(void)level;
	(void)scsc_syserr_code;
	SCSC_TAG_DEBUG(FM, "OK\n");
}

static int stop_close_service(void)
{
	int r;

	if (!fm_client->fm_service) {
		SCSC_TAG_ERR(FM, "No fm_service\n");
		r = -EINVAL;
		goto done;
	}

	r = scsc_mx_service_stop(fm_client->fm_service);
	if (r) {
		SCSC_TAG_ERR(FM, "scsc_mx_service_stop(fm_service) failed %d\n", r);
		goto done;
	}
	SCSC_TAG_DEBUG(FM, "scsc_mx_service_stop(fm_service) OK\n");

	scsc_mx_service_mifram_free(fm_client->fm_service, fm_client->ref);

	r = scsc_mx_service_close(fm_client->fm_service);
	if (r) {
		SCSC_TAG_ERR(FM, "scsc_mx_service_close(fm_service) failed %d\n", r);
		goto done;
	} else
		SCSC_TAG_DEBUG(FM, "scsc_mx_service_close(fm_service) OK\n");

	fm_client->fm_service = NULL;
	fm_client->ref = 0;
done:
	return r;
}

static int open_start_service(void)
{
	struct scsc_service *fm_service;
	int	r;
	int	r2;
	struct fm_ldo_conf *ldo_conf;
	scsc_mifram_ref ref;

	fm_service = scsc_mx_service_open(fm_client->mx, service_id, &fm_client->fm_service_client, &r);
	if (!fm_service) {
		r = -EINVAL;
		SCSC_TAG_ERR(FM, "scsc_mx_service_open(fm_service) failed %d\n", r);
		goto done;
	}
	/* Allocate memory */
	r = scsc_mx_service_mifram_alloc(fm_service, sizeof(struct fm_ldo_conf), &ref, 32);
	if (r) {
		SCSC_TAG_ERR(FM, "scsc_mx_service_mifram_alloc(fm_service) failed %d\n", r);
		r2 = scsc_mx_service_close(fm_service);
		if (r2)
			SCSC_TAG_ERR(FM, "scsc_mx_service_close(fm_service) failed %d\n", r2);
		goto done;
	}
	ldo_conf = (struct fm_ldo_conf *)scsc_mx_service_mif_addr_to_ptr(fm_service, ref);
	ldo_conf->version = FM_LDO_CONFIG_VERSION;
	ldo_conf->ldo_on = fm_client->ldo_on;

	r = scsc_mx_service_start(fm_service, ref);
	if (r) {
		SCSC_TAG_ERR(FM, "scsc_mx_service_start(fm_service) failed %d\n", r);
		r2 = scsc_mx_service_close(fm_service);
		if (r2)
			SCSC_TAG_ERR(FM, "scsc_mx_service_close(fm_service) failed %d\n", r2);
		scsc_mx_service_mifram_free(fm_service, ref);
		goto done;
	}

	fm_client->fm_service = fm_service;
	fm_client->ref = ref;
done:
	return r;
}

static int open_start_close_service(void)
{
	int r;

	r = open_start_service();
	if (r) {
		SCSC_TAG_ERR(FM, "Error starting service: open_start_service(fm_service) failed %d\n", r);

		if (!fm_client->ldo_on) {
			/* Do not return here. For the case where WLBT FW is crashed, and FM off request is
			 * rejected, it's safest to continue to let scsc_service_on_halt_ldos_off() reset
			 * the global flag to indicate that FM is no longer needed when WLBT next boots.
			 * Otherwise LDO could be stuck always-on.
			 */
		} else
			return r;
	}

	if (fm_client->ldo_on) {
		/* FM turning on */
		mxman_fm_on_halt_ldos_on();

	} else {
		/* FM turning off */
		mxman_fm_on_halt_ldos_off();

		/* Invalidate stored FM params */
		mxman_fm_set_params(NULL);
	}

	r = stop_close_service();
	if (r) {
		SCSC_TAG_ERR(FM, "Error starting service: stop_close_service(fm_service) failed %d\n", r);
		return r;
	}
	return 0;
}

static void fm_client_work_func(struct work_struct *work)
{
	SCSC_TAG_DEBUG(FM, "mx250: %s\n", __func__);

	fm_client->fm_client_work_completion_status = open_start_close_service();
	if (fm_client->fm_client_work_completion_status) {
		SCSC_TAG_ERR(FM, "open_start_close_service(fm_service) failed %d\n",
				fm_client->fm_client_work_completion_status);
	} else {
		SCSC_TAG_DEBUG(FM, "OK\n");
	}
	complete(&fm_client->fm_client_work_completion);

}

static void fm_client_wq_init(void)
{
	fm_client->fm_client_wq = create_singlethread_workqueue("fm_client_wq");
	INIT_WORK(&fm_client->fm_client_work, fm_client_work_func);
}

static void fm_client_wq_stop(void)
{
	cancel_work_sync(&fm_client->fm_client_work);
	flush_workqueue(fm_client->fm_client_wq);
}

static void fm_client_wq_deinit(void)
{
	fm_client_wq_stop();
	destroy_workqueue(fm_client->fm_client_wq);
}

static void fm_client_wq_start(void)
{
	queue_work(fm_client->fm_client_wq, &fm_client->fm_client_work);
}

static int fm_client_wq_start_blocking(void)
{
	SCSC_TAG_DEBUG(FM, "mx250: %s\n", __func__);

	fm_client_wq_start();
	wait_for_completion(&fm_client->fm_client_work_completion);
	if (fm_client->fm_client_work_completion_status) {
		SCSC_TAG_ERR(FM, "%s failed: fm_client_wq_completion_status = %d\n",
				__func__, fm_client->fm_client_work_completion_status);
		return fm_client->fm_client_work_completion_status;
	}
	SCSC_TAG_DEBUG(FM, "OK\n");
	return 0;
}

static int mx250_fm_re(bool ldo_on)
{
	int r;

	mutex_lock(&ss_lock);
	SCSC_TAG_DEBUG(FM, "mx250: %s\n", __func__);
	if (!fm_client) {
		SCSC_TAG_ERR(FM, "fm_client = NULL\n");
		mutex_unlock(&ss_lock);
		return -ENODEV;
	}

	if (!fm_client->fm_api_available) {
		SCSC_TAG_WARNING(FM, "FM LDO API unavailable\n");
		mutex_unlock(&ss_lock);
		return -EAGAIN;
	}
	fm_client->ldo_on = ldo_on;
	reinit_completion(&fm_client->fm_client_work_completion);
	r = fm_client_wq_start_blocking();
	mutex_unlock(&ss_lock);
	return r;

}

/*
 * FM Radio is starting, tell WLBT drivers
 */
int mx250_fm_request(void)
{

	SCSC_TAG_INFO(FM, "request\n");
	return mx250_fm_re(true);
}
EXPORT_SYMBOL(mx250_fm_request);

/*
 * FM Radio is stopping, tell WLBT drivers
 */
int mx250_fm_release(void)
{
	SCSC_TAG_INFO(FM, "release\n");
	return mx250_fm_re(false);
}
EXPORT_SYMBOL(mx250_fm_release);

/*
 * FM Radio parameters are changing, tell WLBT drivers
 */
void mx250_fm_set_params(struct wlbt_fm_params *info)
{
	SCSC_TAG_DEBUG(FM, "mx250: %s\n", __func__);

	if (!info)
		return;

	mutex_lock(&ss_lock);

	SCSC_TAG_INFO(FM, "freq %u\n", info->freq);

	mxman_fm_set_params(info);

	mutex_unlock(&ss_lock);
}
EXPORT_SYMBOL(mx250_fm_set_params);

void fm_client_module_probe(struct scsc_mx_module_client *module_client, struct scsc_mx *mx,
		enum scsc_module_client_reason reason)
{
	/* Avoid unused error */
	(void)module_client;

	SCSC_TAG_INFO(FM, "probe\n");

	mutex_lock(&ss_lock);
	if (reason == SCSC_MODULE_CLIENT_REASON_HW_PROBE) {
		fm_client = kzalloc(sizeof(*fm_client), GFP_KERNEL);
		if (!fm_client) {
			mutex_unlock(&ss_lock);
			return;
		}
		init_completion(&fm_client->fm_client_work_completion);
		fm_client_wq_init();
		fm_client->fm_service_client.failure_notification = fm_client_failure_notification;
		fm_client->fm_service_client.stop_on_failure_v2   = fm_client_stop_on_failure;
		fm_client->fm_service_client.failure_reset_v2     = fm_client_failure_reset;
		fm_client->mx = mx;
	}
	fm_client->fm_api_available = true;
	SCSC_TAG_DEBUG(FM, "OK\n");
	mutex_unlock(&ss_lock);
}

void fm_client_module_remove(struct scsc_mx_module_client *module_client, struct scsc_mx *mx,
		enum scsc_module_client_reason reason)
{
	/* Avoid unused error */
	(void)module_client;

	SCSC_TAG_INFO(FM, "remove\n");

	mutex_lock(&ss_lock);
	if (reason == SCSC_MODULE_CLIENT_REASON_HW_REMOVE) {
		if (!fm_client) {
			mutex_unlock(&ss_lock);
			return;
		}
		if (fm_client->mx != mx) {
			SCSC_TAG_ERR(FM, "fm_client->mx != mx\n");
			mutex_unlock(&ss_lock);
			return;
		}
		fm_client_wq_deinit();
		kfree(fm_client);
		fm_client = NULL;
	}
	SCSC_TAG_DEBUG(FM, "OK\n");
	mutex_unlock(&ss_lock);
}

/* FM client driver registration */
struct scsc_mx_module_client fm_client_driver = {
	.name = "FM client driver",
	.probe = fm_client_module_probe,
	.remove = fm_client_module_remove,
};

static int __init scsc_fm_client_module_init(void)
{
	int r;

	SCSC_TAG_INFO(FM, "init\n");

	r = scsc_mx_module_register_client_module(&fm_client_driver);
	if (r) {
		SCSC_TAG_ERR(FM, "scsc_mx_module_register_client_module failed: r=%d\n", r);
		return r;
	}
#ifdef CONFIG_SCSC_FM_TEST
	mx250_fm_test_init();
#endif
	return 0;
}

static void __exit scsc_fm_client_module_exit(void)
{
	SCSC_TAG_INFO(FM, "exit\n");
	scsc_mx_module_unregister_client_module(&fm_client_driver);
#ifdef CONFIG_SCSC_FM_TEST
	mx250_fm_test_exit();
#endif
	SCSC_TAG_DEBUG(FM, "exit\n");
}

late_initcall(scsc_fm_client_module_init);
module_exit(scsc_fm_client_module_exit);

MODULE_DESCRIPTION("FM Client Driver");
MODULE_AUTHOR("SCSC");
MODULE_LICENSE("GPL");
