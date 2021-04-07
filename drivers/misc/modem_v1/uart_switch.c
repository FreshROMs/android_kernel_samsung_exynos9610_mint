/*
 * Copyright (C) 2014 Samsung Electronics Co.Ltd
 * http://www.samsung.com
 *
 * UART Switch driver
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/delay.h>
#include <linux/sec_class.h>
#include <linux/mcu_ipc.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#elif defined(CONFIG_IFCONN_NOTIFIER)
#include <linux/ifconn/ifconn_notifier.h>
#include <linux/ifconn/ifconn_manager.h>
#include <linux/muic/muic_notifier.h>
#include <linux/muic/s2mu004-muic.h>
#endif

#if defined(CONFIG_CCIC_NOTIFIER)
#include <linux/usb/typec/pdic_notifier.h>
#endif

#include <soc/samsung/exynos-pmu.h>
#include "modem_utils.h"
#include "uart_switch.h"

#define UART_DIRECTION_BY_BOOTPARAM 1

enum uart_direction_t uart_dir = AP;
struct uart_switch_data *switch_data;

static void send_uart_noti_to_cp(enum uart_direction_t path)
{
	mif_info("mbox update value %s\n", (path == AP) ? "AP" : "CP");

	mbox_update_value(MCU_CP, switch_data->mbx_ap_united_status, path,
		switch_data->sbi_uart_noti_mask, switch_data->sbi_uart_noti_pos);
	mbox_set_interrupt(MCU_CP, switch_data->int_uart_noti);
}

static int set_uart_switch(enum uart_direction_t path)
{
	int ret = 0;
	u32 reg_val = 0;

	mif_info("Changing path to %s\n", (path == AP) ? "AP" : "CP");

	/*
	 * NOTICE:
	 * Register values are depended on SOC chip
	 * Please set the values by user manual
	 */
	if (path == AP) {
		if (switch_data->use_usb_phy)
			reg_val = 0x00110001;
		else
			reg_val = 0x00120000;
	} else {
		if (switch_data->use_usb_phy)
			reg_val = 0x11001001;
		else
			reg_val = 0x11002000;
	}
	ret = exynos_pmu_write(0x6200, reg_val);
	if (ret < 0) {
		mif_err("ERR(%d) set UART_IO_SHARE_CTRL\n", ret);
		return ret;
	}

	if (switch_data->use_usb_phy) {
		ret = exynos_pmu_update(0x0704, 1, 0x1 << 1);
		if (ret < 0) {
			mif_err("ERR(%d) set USBDEV_PHY_CONTROL\n", ret);
			return ret;
		}
	}

	return 0;
}

void cp_recheck_uart_dir(void)
{
	if (uart_dir != CP) {
		mif_info("uart_dir is not CP\n");
		return;
	}

	uart_dir = CP;
	set_uart_switch(CP);
	send_uart_noti_to_cp(CP);
	mif_info("Forcely changed to CP uart!!\n");
}

#if defined(CONFIG_MUIC_NOTIFIER) && !defined(UART_DIRECTION_BY_BOOTPARAM)
static int switch_handle_notification(struct notifier_block *nb,
				unsigned long action, void *data)
{
#if defined ( CONFIG_CCIC_NOTIFIER)
        CC_NOTI_ATTACH_TYPEDEF *p_noti = (CC_NOTI_ATTACH_TYPEDEF *)data;
        muic_attached_dev_t attached_dev = p_noti->cable_type;
#else
        muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;
#endif
       mif_info("action=%lu attached_dev=%d\n", action, (int)attached_dev);

	switch (attached_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
		break;
	default:
		mif_err("attached device is no JIG\n");
		return 0;
	}

	switch (action) {
	case MUIC_NOTIFY_CMD_DETACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_DETACH:
		uart_dir = AP;
		break;
	case MUIC_NOTIFY_CMD_ATTACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_ATTACH:
		uart_dir = CP;
		set_uart_switch(uart_dir);
		break;
	default:
		mif_err("muic notify cmd error\n");
		return -1;
	}

	send_uart_noti_to_cp(uart_dir);

	return 0;
}
#elif defined(CONFIG_IFCONN_NOTIFIER)
static int switch_handle_notification(struct notifier_block *nb,
				unsigned long action, void *data)
{
	struct ifconn_notifier_template *p_noti = (struct ifconn_notifier_template *)data;
	muic_attached_dev_t attached_dev = (muic_attached_dev_t)p_noti->event;

	mif_info("action=%lu attached_dev=%d\n", action, (int)attached_dev);

	switch (attached_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
		break;
	default:
		mif_err("attached device is no JIG\n");
		return 0;
	}

	switch (action) {
	case IFCONN_NOTIFY_ID_DETACH:
		uart_dir = AP;
		break;
	case IFCONN_NOTIFY_ID_ATTACH:
		uart_dir = CP;
		set_uart_switch(uart_dir);
		break;
	default:
		mif_err("ifconn notify id error\n");
		return -1;
	}

	send_uart_noti_to_cp(uart_dir);

	return 0;
}
#endif

static ssize_t usb_sel_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "PDA\n");
}

static ssize_t usb_sel_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return count;
}

static ssize_t uart_sel_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n",
				(uart_dir == AP ? "AP" : "CP"));
}

static ssize_t uart_sel_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	mif_info("Change UART port path\n");

	if (!strncasecmp(buf, "AP", 2)) {
		uart_dir = AP;
	} else if (!strncasecmp(buf, "CP", 2)) {
		uart_dir = CP;
	} else {
		mif_err("invalid value\n");
		return count;
	}

	set_uart_switch(uart_dir);
	send_uart_noti_to_cp(uart_dir);

	return count;
}

static DEVICE_ATTR(usb_sel, 0664, usb_sel_show, usb_sel_store);
static DEVICE_ATTR(uart_sel, 0664, uart_sel_show, uart_sel_store);

static struct attribute *uart_sel_attributes[] = {
	&dev_attr_uart_sel.attr,
	&dev_attr_usb_sel.attr,
	NULL
};

static const struct attribute_group uart_sel_group = {
	.attrs = uart_sel_attributes,
};

static int uart_switch_setup(char *str)
{
	uart_dir = strstr(str, "CP") ? CP : AP;
	mif_info("uart direction: %s (%d)\n", str, uart_dir);

	return 0;
}
__setup("uart_switch=", uart_switch_setup);

int uart_switch_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	mif_info("uart_switch init start.\n");

	switch_data = devm_kzalloc(dev, sizeof(struct uart_switch_data), GFP_KERNEL);
	if (!switch_data) {
		mif_err_limited("switch_data failed to alloc memory\n");
		return -ENOMEM;
	}
	switch_data->dev = dev;

	if (of_property_read_u32(dev->of_node, "mif,int_ap2cp_uart_noti",
			&switch_data->int_uart_noti)) {
		mif_err("int_ap2cp_uart_noti parse error!\n");
		goto init_err;
	}

	if (of_property_read_u32(dev->of_node, "mbx_ap2cp_united_status",
			&switch_data->mbx_ap_united_status)) {
		mif_err("mbox_ap2cp_united_status parse error!\n");
		goto init_err;
	}

	if (of_property_read_u32(dev->of_node, "sbi_uart_noti_mask",
			&switch_data->sbi_uart_noti_mask)) {
		mif_err("sbi_uart_noti_mask parse error!\n");
		goto init_err;
	}

	if (of_property_read_u32(dev->of_node, "sbi_uart_noti_pos",
			&switch_data->sbi_uart_noti_pos)) {
		mif_err("sbi_uart_noti_pos parse error!\n");
		goto init_err;
	}

	if (of_property_read_u32(dev->of_node, "mif,use_usb_phy",
			&switch_data->use_usb_phy)) {
		mif_err("use_usb_phy parse error!\n");
		goto init_err;
	}

	mif_info("use_usb_phy [%d]\n", switch_data->use_usb_phy);

#if defined(CONFIG_MUIC_NOTIFIER) && !defined(UART_DIRECTION_BY_BOOTPARAM)
	switch_data->uart_notifier.notifier_call = switch_handle_notification;
	muic_notifier_register(&switch_data->uart_notifier, switch_handle_notification,
					MUIC_NOTIFY_DEV_MODEM);
#elif defined(CONFIG_IFCONN_NOTIFIER)
	switch_data->uart_notifier.notifier_call = switch_handle_notification;
	ifconn_notifier_register(&switch_data->uart_notifier, switch_handle_notification,
					IFCONN_NOTIFY_MODEM, IFCONN_NOTIFY_MUIC);
#endif

	/* create sysfs group */
	if (sysfs_create_group(&switch_device->kobj, &uart_sel_group)) {
		mif_err("failed to create modemif sysfs attribute group\n");
		goto init_err;
	}

#if defined(UART_DIRECTION_BY_BOOTPARAM)
	if (get_switch_sel() & SWITCH_SEL_UART_MASK)
		uart_dir = AP;
	else
		uart_dir = CP;

	set_uart_switch(uart_dir);
#endif
	return 0;

init_err:
	if (switch_data)
		devm_kfree(dev, switch_data);

	return -EINVAL;
}
