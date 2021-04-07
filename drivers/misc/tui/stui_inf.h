/*
 * Samsung TUI HW Handler driver.
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __LINUX_SAMSUNG_TUI_INF_H
#define __LINUX_SAMSUNG_TUI_INF_H

#define STUI_MODE_OFF			0x00
#define STUI_MODE_TUI_SESSION		0x01
#define STUI_MODE_DISPLAY_SEC		0x02
#define STUI_MODE_TOUCH_SEC		0x04
#define STUI_MODE_ALL			(STUI_MODE_TUI_SESSION | STUI_MODE_DISPLAY_SEC | STUI_MODE_TOUCH_SEC)

int  stui_inc_blank_ref(void);
int  stui_dec_blank_ref(void);
int  stui_get_blank_ref(void);
void stui_set_blank_ref(int ref);

int  stui_get_mode(void);
void stui_set_mode(int mode);

int  stui_set_mask(int mask);
int  stui_clear_mask(int mask);

int stui_cancel_session(void);

#ifdef CONFIG_SOC_EXYNOS3250
#define TRUSTEDUI_MODE_CHANGE		1
#define TRUSTEDUI_MODE_OFF				STUI_MODE_OFF
#define TRUSTEDUI_MODE_INPUT_SECURED	STUI_MODE_TOUCH_SEC
#define TRUSTEDUI_MODE_ALL				STUI_MODE_ALL

int trustedui_nb_register(struct notifier_block *nb);
int trustedui_nb_unregister(struct notifier_block *nb);
int trustedui_nb_send_event(unsigned long val, void *v);
void trustedui_set_mode(int mode);
#endif /* CONFIG_SOC_EXYNOS3250 */

#endif /* __LINUX_SAMSUNG_TUI_INF_H */
