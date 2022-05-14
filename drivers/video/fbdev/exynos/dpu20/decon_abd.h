/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DECON_ABD_H__
#define __DECON_ABD_H__

#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>

#if defined(CONFIG_EXYNOS_DPU30)
#include <linux/dma-fence.h>
#endif

#define ABD_LOG_MAX	50

struct fb_ops;

struct str_log {
	u64 stamp;
	u64 ktime;

	/* string */
	const char *print;
};

struct abd_str {
	const char *name;
	unsigned int count;
	unsigned int lcdon_flag;

	struct str_log log[ABD_LOG_MAX];
};

struct pin_log {
	u64 stamp;
	u64 ktime;

	/* pin */
	unsigned int level;
	unsigned int state;
	unsigned int onoff;
};

struct abd_pin {
	const char *name;
	unsigned int count;
	unsigned int lcdon_flag;

	struct pin_log log[ABD_LOG_MAX];
};

struct abd_pin_info {
	const char *name;
	unsigned int irq;
	struct irq_desc *desc;
	int gpio;
	int level;
	int active_level;
	unsigned int enable;

	struct abd_pin p_first;
	struct abd_pin p_lcdon;
	struct abd_pin p_event;

	struct list_head handler_list;
};

struct adb_pin_handler {
	struct list_head node;
	irq_handler_t handler;
	void *dev_id;
};

struct fto_log {
	u64 stamp;
	u64 ktime;

	/* fence */
	unsigned int winid;
#if defined(CONFIG_SOC_EXYNOS7885)
	struct sync_fence fence;
#elif defined(CONFIG_SOC_EXYNOS9810)
	struct sync_file fence;
#else
	struct dma_fence fence;
#endif
};

struct abd_fto {
	const char *name;
	unsigned int count;
	unsigned int lcdon_flag;

	struct fto_log log[ABD_LOG_MAX];
};

struct udr_log {
	u64 stamp;
	u64 ktime;

	/* underrun */
	unsigned long mif;
	unsigned long iint;
	unsigned long disp;
	unsigned long aclk;
	void *bts;
};

struct abd_udr {
	const char *name;
	unsigned int count;
	unsigned int lcdon_flag;

	struct udr_log log[ABD_LOG_MAX];
};

struct bit_log {
	u64 stamp;
	u64 ktime;

	/* bit error */
	unsigned int size;
	unsigned int value;
	char *print[32];	/* max: 32 bit */
};

struct abd_bit {
	const char *name;
	unsigned int count;
	unsigned int lcdon_flag;

	struct bit_log log[ABD_LOG_MAX];
};

enum {
	ABD_PIN_PCD,
	ABD_PIN_DET,
	ABD_PIN_ERR,
	ABD_PIN_CON,
	ABD_PIN_LOG,	/* reserved for just log */
	ABD_PIN_MAX
};

struct abd_protect {
	struct abd_pin_info pin[ABD_PIN_MAX];

	struct abd_fto f_first;
	struct abd_fto f_lcdon;
	struct abd_fto f_event;

	struct abd_udr u_first;
	struct abd_udr u_lcdon;
	struct abd_udr u_event;

	struct abd_bit b_first;
	struct abd_bit b_event;

	struct abd_str s_event;

	struct notifier_block pin_early_notifier;
	struct notifier_block pin_after_notifier;

	unsigned int enable;
	struct notifier_block reboot_notifier;
	spinlock_t slock;

	struct workqueue_struct *con_workqueue;
	struct work_struct con_work;
	unsigned int con_irq;
	unsigned int con_blank;

	struct notifier_block con_fb_notifier;
	struct fb_ops fbops;

	struct device *fbdev;
	unsigned int wake_lock_enable;

	struct dentry *debugfs_root;
	unsigned int init_done;
	
	struct miscdevice misc_entry;
	struct mutex misc_lock;
};

#if defined(CONFIG_EXYNOS_DPU20)
extern unsigned int lcdtype;
#elif defined(CONFIG_EXYNOS_DPU30)
extern int boot_panel_id;
#endif

extern void decon_abd_save_fto(struct abd_protect *abd, void *fence);
extern void decon_abd_save_udr(struct abd_protect *abd, unsigned long mif, unsigned long iint, unsigned long disp);
extern void decon_abd_save_str(struct abd_protect *abd, const char *print);
extern void decon_abd_save_bit(struct abd_protect *abd, unsigned int size, unsigned int value, char **print);
extern void decon_abd_enable(struct abd_protect *abd, unsigned int enable);
extern int decon_abd_pin_enable(struct abd_protect *abd, unsigned int gpio, bool on);
extern int decon_abd_pin_register_handler(struct abd_protect *abd, int irq, irq_handler_t handler, void *dev_id);
extern int decon_abd_pin_unregister_handler(struct abd_protect *abd, int irq, irq_handler_t handler, void *dev_id);
extern int decon_abd_con_register(struct abd_protect *abd);
extern struct device *find_lcd_class_device(void);
extern struct platform_device *of_find_abd_dt_parent_platform_device(void);
extern struct platform_device *of_find_abd_container_platform_device(void);
struct decon_device;
#if defined(CONFIG_LOGGING_BIGDATA_BUG)
extern void log_decon_bigdata(struct decon_device *decon);
#else
static inline void log_decon_bigdata(struct decon_device *decon) {};
#endif
#endif

