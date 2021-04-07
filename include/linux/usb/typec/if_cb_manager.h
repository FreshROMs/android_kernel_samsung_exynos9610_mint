/*
 * Copyright (C) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *
 */

#ifndef __IF_CB_MANAGER_H__
#define __IF_CB_MANAGER_H__

struct muic_ops {
	int (*muic_check_usb_killer)(void *data);
};

struct usbpd_ops {
	int (*usbpd_sbu_test_read)(void *data);
};

struct muic_dev {
	const struct muic_ops *ops;
	void *data;
};

struct usbpd_dev {
	const struct usbpd_ops *ops;
	void *data;
};

struct if_cb_manager {
	struct muic_dev *muic_d;
	struct usbpd_dev *usbpd_d;
};

extern struct if_cb_manager *register_muic(struct muic_dev *muic);
extern struct if_cb_manager *register_usbpd(struct usbpd_dev *usbpd);
extern int muic_check_usb_killer(struct if_cb_manager *man_core);
extern int usbpd_sbu_test_read(struct if_cb_manager *man_core);

#endif /* __IF_CB_MANAGER_H__ */
