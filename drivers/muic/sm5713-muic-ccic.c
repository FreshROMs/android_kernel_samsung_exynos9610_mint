/*
 * sm5713-muic_ccic.c
 *
 * Copyright (C) 2018 Samsung Electronics
 * Heegon Lee <heegon.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/host_notify.h>
#include <linux/string.h>
#include <linux/wakelock.h>

#include <linux/muic/muic.h>
#if defined(CONFIG_SEC_FACTORY)
#include <linux/mfd/sm5713-private.h>
#endif
#include <linux/muic/sm5713-muic.h>
#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif
#include <linux/battery/battery_notifier.h>

#if defined(CONFIG_MUIC_SUPPORT_CCIC)
#include <linux/usb/typec/pdic_notifier.h>
#endif
#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#include <linux/usb/typec/usb_typec_manager_notifier.h>
#endif

static void sm5713_muic_init_ccic_info_data(struct sm5713_muic_data *muic_data)
{
	pr_info("%s\n", __func__);
	muic_data->ccic_info_data.ccic_evt_rid = RID_OPEN;
	muic_data->ccic_info_data.ccic_evt_rprd = 0;
	muic_data->ccic_info_data.ccic_evt_roleswap = 0;
	muic_data->ccic_info_data.ccic_evt_dcdcnt = 0;
	muic_data->ccic_info_data.ccic_evt_attached = MUIC_CCIC_NOTI_UNDEFINED;
	muic_data->ccic_afc_state = SM5713_MUIC_AFC_NORMAL;
}

static void sm5713_muic_handle_ccic_detach(struct sm5713_muic_data *muic_data)
{
	pr_info("%s\n", __func__);
	muic_data->ccic_info_data.ccic_evt_rprd = 0;
	muic_data->ccic_info_data.ccic_evt_roleswap = 0;
	muic_data->ccic_info_data.ccic_evt_dcdcnt = 0;
	muic_data->ccic_info_data.ccic_evt_attached = MUIC_CCIC_NOTI_DETACH;
	muic_data->ccic_afc_state = SM5713_MUIC_AFC_NORMAL;
}

static void sm5713_muic_handle_ccic_ABNORMAL(struct sm5713_muic_data *muic_data)
{
	muic_data->ccic_afc_state = SM5713_MUIC_AFC_ABNORMAL;

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_12V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		muic_afc_set_voltage(5);
		break;
	default:
		break;
	}
}

static void sm5713_muic_handle_RPLEVEL(struct sm5713_muic_data *muic_data)
{
	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_12V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		break;
	default:
		muic_data->ccic_afc_state = SM5713_MUIC_AFC_ABNORMAL;
		break;
	}
}

static int sm5713_muic_handle_ccic_ATTACH(struct sm5713_muic_data *muic_data,
		CC_NOTI_ATTACH_TYPEDEF *pnoti)
{
	bool need_to_run_work = false;

	pr_info("%s: src:%d dest:%d id:%d attach:%d cable_type:%d rprd:%d\n",
			__func__, pnoti->src, pnoti->dest, pnoti->id,
			pnoti->attach, pnoti->cable_type, pnoti->rprd);

	/* Attached */
	if (pnoti->attach) {
		pr_info("%s: Attach, cable type=%d\n", __func__,
				pnoti->cable_type);

		muic_data->ccic_info_data.ccic_evt_attached =
			MUIC_CCIC_NOTI_ATTACH;

		if (muic_data->ccic_info_data.ccic_evt_roleswap) {
			pr_info("%s: roleswap event, attach USB\n", __func__);
			muic_data->ccic_info_data.ccic_evt_roleswap = 0;
			need_to_run_work = true;
		}

		if (pnoti->rprd) {
			pr_info("%s: RPRD\n", __func__);
			muic_data->ccic_info_data.ccic_evt_rprd = 1;
			need_to_run_work = true;
		}

		if (pnoti->cable_type == RP_CURRENT_ABNORMAL)
			sm5713_muic_handle_ccic_ABNORMAL(muic_data);
		else if (pnoti->cable_type == RP_CURRENT_LEVEL2 ||
				pnoti->cable_type == RP_CURRENT_LEVEL3)
			sm5713_muic_handle_RPLEVEL(muic_data);

		if (pnoti->cable_type == ATTACHED_DEV_TIMEOUT_OPEN_MUIC) {
			muic_data->ccic_info_data.ccic_evt_dcdcnt = 1;
			need_to_run_work = true;
		}

#if defined(CONFIG_MUIC_BCD_RESCAN)
		if (muic_data->bc12_retry_skip && pnoti->cable_type) {
			muic_data->bc12_retry_skip = 0;
			need_to_run_work = true;
		}
#endif

		muic_data->is_water_detect = false;
	} else {
		if (pnoti->rprd) {
			/* Role swap detach: attached=0, rprd=1 */
			pr_info("%s: role swap event\n", __func__);
			muic_data->ccic_info_data.ccic_evt_roleswap = 1;
		} else {
			/* Detached */
			if (muic_data->ccic_info_data.ccic_evt_rprd)
				need_to_run_work = true;

			if (muic_data->need_to_path_open)
				need_to_run_work = true;

			if (muic_data->ccic_info_data.ccic_evt_dcdcnt)
				need_to_run_work = true;

			sm5713_muic_handle_ccic_detach(muic_data);
		}
	}

	/* run muic event handler */
	if (need_to_run_work) {
		pr_info("%s: do workqueue\n", __func__);
		schedule_work(&(muic_data->muic_event_work));
	}

	return 0;
}

static int sm5713_muic_handle_ccic_RID(struct sm5713_muic_data *muic_data,
		CC_NOTI_RID_TYPEDEF *pnoti)
{
	int prev_rid = muic_data->ccic_info_data.ccic_evt_rid;
	int rid = pnoti->rid;
#if defined(CONFIG_SEC_FACTORY)
	int intr2 = 0;
#endif

	pr_info("%s: src:%d dest:%d id:%d rid:%d sub2:%d sub3:%d\n",
			__func__, pnoti->src, pnoti->dest, pnoti->id,
			pnoti->rid, pnoti->sub2, pnoti->sub3);

	if (rid > RID_OPEN || rid <= RID_UNDEFINED) {
		pr_info("%s: Out of range of RID(%d)\n", __func__, rid);
		return 0;
	}

	muic_data->ccic_info_data.ccic_evt_rid = rid;

	switch (rid) {
	case RID_000K:
	case RID_255K:
	case RID_301K:
	case RID_523K:
	case RID_619K:
#if defined(CONFIG_SEC_FACTORY)
		if (!muic_data->afc_irq_disabled) {
			disable_irq(muic_data->irqs.irq_afc_ta_attached);
			pr_info("%s: disable_irq (%d)\n", __func__,
					muic_data->irqs.irq_afc_ta_attached);
			muic_data->afc_irq_disabled = true;
		}
#endif
		break;
	case RID_OPEN:
#if defined(CONFIG_SEC_FACTORY)
		if (muic_data->afc_irq_disabled) {
			intr2 = sm5713_i2c_read_byte(muic_data->i2c,
					SM5713_MUIC_REG_INT2);
			pr_info("%s: REG_INT2 (0x%x)\n", __func__, intr2);

			enable_irq(muic_data->irqs.irq_afc_ta_attached);
			pr_info("%s: enable_irq (%d)\n", __func__,
					muic_data->irqs.irq_afc_ta_attached);
			muic_data->afc_irq_disabled = false;
		}
#endif
		break;
	default:
		pr_err("%s:Not determined now\n", __func__);
		break;
	}

	if (prev_rid != rid) {
		pr_info("%s: do workqueue\n", __func__);
		schedule_work(&(muic_data->muic_event_work));
	}

	return 0;
}

static int sm5713_muic_handle_ccic_WATER(struct sm5713_muic_data *muic_data,
		CC_NOTI_ATTACH_TYPEDEF *pnoti)
{
	pr_info("%s: src:%d dest:%d id:%d attach:%d cable_type:%d rprd:%d\n",
			__func__, pnoti->src, pnoti->dest, pnoti->id,
			pnoti->attach, pnoti->cable_type, pnoti->rprd);

	if (pnoti->attach == CCIC_NOTIFY_ATTACH) {
		muic_data->is_water_detect = true;
		muic_set_hiccup_mode(0);
		pr_info("%s: Water detect\n", __func__);
	} else {
		muic_data->is_water_detect = false;
		pr_info("%s: Dry detect\n", __func__);
	}

	return 0;
}

static int sm5713_muic_handle_ccic_notification(struct notifier_block *nb,
				unsigned long action, void *data)
{
	CC_NOTI_TYPEDEF *pnoti = (CC_NOTI_TYPEDEF *)data;
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	struct sm5713_muic_data *muic_data = container_of(nb,
			struct sm5713_muic_data, manager_nb);
#else
	struct sm5713_muic_data *muic_data = container_of(nb,
			struct sm5713_muic_data, ccic_nb);
#endif

	pr_info("%s: action:%d src:%d dest:%d id:%d sub[%d %d %d]\n", __func__,
		(int)action, pnoti->src, pnoti->dest, pnoti->id,
		pnoti->sub1, pnoti->sub2, pnoti->sub3);

#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	if (pnoti->dest != CCIC_NOTIFY_DEV_MUIC) {
		pr_info("%s destination id is invalid\n", __func__);
		return 0;
	}
#endif
	muic_data->ccic_evt_id = pnoti->id;

	switch (pnoti->id) {
	case CCIC_NOTIFY_ID_ATTACH:
		pr_info("%s: CCIC_NOTIFY_ID_ATTACH: %s\n", __func__,
				pnoti->sub1 ? "Attached" : "Detached");
		sm5713_muic_handle_ccic_ATTACH(muic_data,
				(CC_NOTI_ATTACH_TYPEDEF *)pnoti);
		break;
	case CCIC_NOTIFY_ID_RID:
		pr_info("%s: CCIC_NOTIFY_ID_RID\n", __func__);
		sm5713_muic_handle_ccic_RID(muic_data,
				(CC_NOTI_RID_TYPEDEF *)pnoti);
		break;
	case CCIC_NOTIFY_ID_WATER:
		pr_info("%s: CCIC_NOTIFY_ID_WATER\n", __func__);
		sm5713_muic_handle_ccic_WATER(muic_data,
				(CC_NOTI_ATTACH_TYPEDEF *)pnoti);
		break;
	default:
		pr_info("%s: Undefined Noti. ID\n", __func__);
		return NOTIFY_DONE;
	}

	return NOTIFY_DONE;
}

void sm5713_muic_register_ccic_notifier(struct sm5713_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s: Registering CCIC_NOTIFY_DEV_MUIC.\n", __func__);

	sm5713_muic_init_ccic_info_data(muic_data);
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	ret = manager_notifier_register(&muic_data->manager_nb,
		sm5713_muic_handle_ccic_notification, MANAGER_NOTIFY_CCIC_MUIC);
#else
	ret = ccic_notifier_register(&muic_data->ccic_nb,
		sm5713_muic_handle_ccic_notification, CCIC_NOTIFY_DEV_MUIC);
#endif
	if (ret < 0) {
		pr_info("%s: CCIC Noti. is not ready\n", __func__);
		return;
	}

	pr_info("%s: done.\n", __func__);
}

void sm5713_muic_unregister_ccic_notifier(struct sm5713_muic_data *muic_data)
{
	int ret = 0;

#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	ret = manager_notifier_unregister(&muic_data->manager_nb);
#else
	ret = ccic_notifier_unregister(&muic_data->ccic_nb);
#endif
	if (ret < 0) {
		pr_info("%s: CCIC Noti. is not ready\n", __func__);
		return;
	}

	pr_info("%s: done.\n", __func__);
}
