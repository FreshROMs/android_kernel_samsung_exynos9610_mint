/*
 * Copyright (C) 2014 Samsung Electronics Co.Ltd
 * http://www.samsung.com
 *
 * UART SWITCH driver
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __UART_SWITCH_H__
#define __UART_SWITCH_H__

extern struct device *switch_device;

enum connect_type {
	USB = 0,
	UART = 1,
};

enum uart_direction_t {
	AP = 0,
	CP = 1,
};

struct uart_switch_data {
	struct device *dev;
	char *name;

	bool uart_connect;
	bool uart_switch_sel;

#if defined(CONFIG_MUIC_NOTIFIER) || defined(CONFIG_IFCONN_NOTIFIER)
	struct notifier_block uart_notifier;
#endif
	unsigned int int_uart_noti;
	unsigned int mbx_ap_united_status;
	unsigned int sbi_uart_noti_mask;
	unsigned int sbi_uart_noti_pos;
	unsigned int use_usb_phy;
};

#if defined(CONFIG_UART_SWITCH)
int uart_switch_init(struct platform_device *pdev);
void cp_recheck_uart_dir(void);
#else
static inline int uart_switch_init(struct platform_device *pdev) { return 0; }
static inline void cp_recheck_uart_dir(void) { return; }
#endif

#endif
