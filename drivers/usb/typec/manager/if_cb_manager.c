/*
 * Copyright (C) 2017-2018 Samsung Electronics
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
 * along with this program; If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/usb/typec/if_cb_manager.h>
#include <linux/slab.h>
#include <linux/module.h>

static struct if_cb_manager *man_core;

struct if_cb_manager *create_alloc_if_cb_manager(void)
{
	man_core = kzalloc(sizeof(struct if_cb_manager), GFP_KERNEL);

	return man_core;
}

struct if_cb_manager *get_if_cb_manager(void)
{
	if (!man_core)
		create_alloc_if_cb_manager();

	return man_core;
}

struct if_cb_manager *register_muic(struct muic_dev *muic)
{
	struct if_cb_manager *man_core;

	man_core = get_if_cb_manager();
	man_core->muic_d = muic;

	return man_core;
}

struct if_cb_manager *register_usbpd(struct usbpd_dev *usbpd)
{
	struct if_cb_manager *man_core;

	man_core = get_if_cb_manager();
	man_core->usbpd_d = usbpd;

	return man_core;
}

int muic_check_usb_killer(struct if_cb_manager *man_core)
{
	if (man_core == NULL || man_core->muic_d == NULL ||
			man_core->muic_d->ops == NULL ||
			man_core->muic_d->ops->muic_check_usb_killer == NULL)
		return 0;

	return man_core->muic_d->ops->muic_check_usb_killer(
			man_core->muic_d->data);
}

int usbpd_sbu_test_read(struct if_cb_manager *man_core)
{
	if (man_core == NULL || man_core->usbpd_d == NULL ||
			man_core->usbpd_d->ops == NULL ||
			man_core->usbpd_d->ops->usbpd_sbu_test_read == NULL)
		return -ENXIO;

	return man_core->usbpd_d->ops->usbpd_sbu_test_read(
			man_core->usbpd_d->data);
}

MODULE_LICENSE("GPL");
