// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk-provider.h>
#include <linux/debugfs.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/lcd.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reboot.h>
#include <linux/rtc.h>

#include <media/v4l2-subdev.h>
#include "../../../../../kernel/irq/internals.h"
#if defined(CONFIG_ARCH_EXYNOS)
#include <linux/exynos_iovmm.h>
#include <soc/samsung/exynos-devfreq.h>
#endif
#if defined(CONFIG_CAL_IF)
#include <soc/samsung/cal-if.h>
#endif
#if defined(CONFIG_SOC_EXYNOS7885)
#include <dt-bindings/clock/exynos7885.h>
#endif
#if defined(CONFIG_SOC_EXYNOS9610)
#include <dt-bindings/clock/exynos9610.h>
#endif
#if defined(CONFIG_SOC_EXYNOS9810)
#include <dt-bindings/clock/exynos9810.h>
#endif
#if defined(CONFIG_SOC_EXYNOS3830)
#include <dt-bindings/soc/samsung/exynos3830-devfreq.h>
#endif
#if defined(CONFIG_SOC_EXYNOS9630)
#include <dt-bindings/soc/samsung/exynos9630-devfreq.h>
#endif
#if defined(CONFIG_SEC_DEBUG)
#include <linux/sec_debug.h>
#endif

#if defined(CONFIG_ARCH_EXYNOS) && defined(CONFIG_EXYNOS_DPU20)
#include "decon.h"
#include "dpp.h"
#include "dsim.h"
#elif defined(CONFIG_ARCH_EXYNOS) && defined(CONFIG_EXYNOS_DPU30)
#include "../dpu30/decon.h"
#include "../dpu30/dpp.h"
#include "../dpu30/dsim.h"
#include "panel.h"
#endif

#include "decon_abd.h"
#include "decon_board.h"
#include "decon_notify.h"

#define dbg_info(fmt, ...)		pr_info(pr_fmt("decon: "fmt), ##__VA_ARGS__)
#define dbg_none(fmt, ...)		pr_debug(pr_fmt("decon: "fmt), ##__VA_ARGS__)

#define abd_printf(m, ...)	\
{	if (m) seq_printf(m, __VA_ARGS__); else dbg_info(__VA_ARGS__);	}	\

#if defined(CONFIG_ARCH_EXYNOS) && defined(CONFIG_LOGGING_BIGDATA_BUG)
/* Gen Big Data Error for Decon's Bug
 *
 * return value
 * 1. 31 ~ 28 : decon_id
 * 2. 27 ~ 24 : decon eing pend register
 * 3. 23 ~ 16 : dsim underrun count
 * 4. 15 ~  8 : 0x0e panel register
 * 5.  7 ~  0 : 0x0a panel register
 */

static unsigned int gen_decon_bug_bigdata(struct decon_device *decon)
{
	struct dsim_device *dsim;
	unsigned int value, panel_value;
	unsigned int underrun_cnt = 0;

	/* for decon id */
	value = decon->id << 28;

	if (decon->id == 0) {
		/* for eint pend value */
		value |= (decon->eint_pend & 0x0f) << 24;

		/* for underrun count */
		dsim = container_of(decon->out_sd[0], struct dsim_device, sd);
		if (dsim != NULL) {
			underrun_cnt = dsim->total_underrun_cnt;
			if (underrun_cnt > 0xff) {
				dbg_info("%s: underrun exceed 1byte: %d\n",
						__func__, underrun_cnt);
				underrun_cnt = 0xff;
			}
		}
		value |= underrun_cnt << 16;

		/* for panel dump */
		panel_value = call_panel_ops(dsim, get_buginfo, dsim);
		value |= panel_value & 0xffff;
	}

	dbg_info("%s: big data: %x\n", __func__, value);

	return value;
}

void log_decon_bigdata(struct decon_device *decon)
{
	unsigned int bug_err_num;

	bug_err_num = gen_decon_bug_bigdata(decon);
#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
	sec_debug_set_extra_info_decon(bug_err_num);
#endif
}
#endif

#if defined(CONFIG_ARCH_EXYNOS)
struct platform_device *of_find_abd_dt_parent_platform_device(void)
{
	return of_find_dsim_platform_device();
}

struct platform_device *of_find_abd_container_platform_device(void)
{
	return of_find_decon_platform_device();
}

static struct decon_device *find_container(void)
{
	struct platform_device *pdev = NULL;
	struct decon_device *container = NULL;

	pdev = of_find_abd_container_platform_device();
	if (!pdev) {
		dbg_info("%s: of_find_device_by_node fail\n", __func__);
		return NULL;
	}

	container = platform_get_drvdata(pdev);
	if (!container) {
		dbg_info("%s: platform_get_drvdata fail\n", __func__);
		return NULL;
	}

	return container;
}

static inline struct decon_device *get_abd_container_of(struct abd_protect *abd)
{
	struct decon_device *container = container_of(abd, struct decon_device, abd);

	return container;
}

#if defined(CONFIG_SOC_EXYNOS7885) || defined(CONFIG_SOC_EXYNOS9610)
static void set_frame_bypass(struct abd_protect *abd, unsigned int bypass)
{
	struct decon_device *container = get_abd_container_of(abd);

	if (bypass)
		dbg_info("%s: %d\n", __func__, bypass);

	container->ignore_vsync = bypass;
}

static int get_frame_bypass(struct abd_protect *abd)
{
	struct decon_device *container = get_abd_container_of(abd);

	return container->ignore_vsync;
}
#else
static void set_frame_bypass(struct abd_protect *abd, unsigned int bypass)
{
	struct decon_device *container = get_abd_container_of(abd);

	if (bypass)
		dbg_info("%s: %d\n", __func__, bypass);

	atomic_set(&container->bypass, bypass);
}

static int get_frame_bypass(struct abd_protect *abd)
{
	struct decon_device *container = get_abd_container_of(abd);

	return atomic_read(&container->bypass);
}
#endif

#if defined(CONFIG_EXYNOS_DPU20)
static void set_mipi_rw_bypass(struct abd_protect *abd, unsigned int flag)
{
	struct decon_device *container = get_abd_container_of(abd);
	struct dsim_device *dsim = v4l2_get_subdevdata(container->out_sd[0]);

	dsim->priv.lcdconnected = !flag;
}

static int get_mipi_rw_bypass(struct abd_protect *abd)
{
	struct decon_device *container = get_abd_container_of(abd);
	struct dsim_device *dsim = v4l2_get_subdevdata(container->out_sd[0]);

	return !dsim->priv.lcdconnected;
}

static inline int get_boot_lcdtype(void)
{
	return (int)lcdtype;
}

static inline unsigned int get_boot_lcdconnected(void)
{
	return get_boot_lcdtype() ? 1 : 0;
}
#else
static void set_mipi_rw_bypass(struct abd_protect *abd, unsigned int flag)
{
}

static int get_mipi_rw_bypass(struct abd_protect *abd)
{
	return 0;
}

static inline int get_boot_lcdtype(void)
{
	return boot_panel_id;
}

static inline unsigned int get_boot_lcdconnected(void)
{
	return (get_boot_lcdtype() >= 0) ? 1 : 0;
}
#endif

static struct fb_info *get_fbinfo(struct abd_protect *abd)
{
	struct decon_device *container = get_abd_container_of(abd);

	return container->win[container->dt.dft_win]->fbinfo;
}
#endif

void decon_abd_save_str(struct abd_protect *abd, const char *print)
{
	struct abd_str *event = NULL;
	struct str_log *event_log = NULL;

	if (!abd || !abd->init_done)
		return;

	event = &abd->s_event;
	event_log = &event->log[(event->count % ABD_LOG_MAX)];

	event_log->stamp = local_clock();
	event_log->ktime = ktime_get_real_seconds();
	event_log->print = print;

	event->count++;

	abd_printf(NULL, "%s\n", print);
}

static void _decon_abd_print_bit(struct seq_file *m, struct bit_log *log)
{
	struct timeval tv;
	struct rtc_time tm;
	unsigned int bit = 0;
	char print_buf[200] = {0, };
	struct seq_file p = {
		.buf = print_buf,
		.size = sizeof(print_buf) - 1,
	};

	if (!m)
		seq_puts(&p, "decon_abd: ");

	tv = ns_to_timeval(log->stamp);
	rtc_time_to_tm(log->ktime, &tm);
	seq_printf(&p, "%d-%02d-%02d %02d:%02d:%02d / %lu.%06lu / 0x%0*X, ",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
		(unsigned long)tv.tv_sec, tv.tv_usec, log->size >> 2, log->value);

	for (bit = 0; bit < log->size; bit++) {
		if (log->print[bit]) {
			if (!bit || !log->print[bit - 1] || strcmp(log->print[bit - 1], log->print[bit]))
				seq_printf(&p, "%s, ", log->print[bit]);
		}
	}

	abd_printf(m, "%s\n", p.count ? p.buf : "");
}

void decon_abd_save_bit(struct abd_protect *abd, unsigned int size, unsigned int value, char **print)
{
	struct abd_bit *first = NULL;
	struct abd_bit *event = NULL;

	struct bit_log *first_log = NULL;
	struct bit_log *event_log = NULL;

	if (!abd || !abd->init_done)
		return;

	first = &abd->b_first;
	event = &abd->b_event;

	first_log = &first->log[(first->count % ABD_LOG_MAX)];
	event_log = &event->log[(event->count % ABD_LOG_MAX)];

	memset(event_log, 0, sizeof(struct bit_log));
	event_log->stamp = local_clock();
	event_log->ktime = ktime_get_real_seconds();
	event_log->value = value;
	event_log->size = size;
	memcpy(&event_log->print, print, sizeof(char *) * size);

	if (!first->count) {
		memset(first_log, 0, sizeof(struct bit_log));
		memcpy(first_log, event_log, sizeof(struct bit_log));
		first->count++;
	}

	_decon_abd_print_bit(NULL, event_log);

	event->count++;
}

void decon_abd_save_fto(struct abd_protect *abd, void *fence)
{
	struct abd_fto *first = NULL;
	struct abd_fto *lcdon = NULL;
	struct abd_fto *event = NULL;

	struct fto_log *first_log = NULL;
	struct fto_log *lcdon_log = NULL;
	struct fto_log *event_log = NULL;

	if (!abd || !abd->init_done)
		return;

	first = &abd->f_first;
	lcdon = &abd->f_lcdon;
	event = &abd->f_event;

	first_log = &first->log[(first->count % ABD_LOG_MAX)];
	lcdon_log = &lcdon->log[(lcdon->count % ABD_LOG_MAX)];
	event_log = &event->log[(event->count % ABD_LOG_MAX)];

	memset(event_log, 0, sizeof(struct fto_log));
	event_log->stamp = local_clock();
	event_log->ktime = ktime_get_real_seconds();
#if defined(CONFIG_SOC_EXYNOS7885)
	memcpy(&event_log->fence, fence, sizeof(struct sync_fence));
#elif defined(CONFIG_SOC_EXYNOS9810)
	memcpy(&event_log->fence, fence, sizeof(struct sync_file));
#else
	memcpy(&event_log->fence, fence, sizeof(struct dma_fence));
#endif

	if (!first->count) {
		memset(first_log, 0, sizeof(struct fto_log));
		memcpy(first_log, event_log, sizeof(struct fto_log));
		first->count++;
	}

	if (!lcdon->lcdon_flag) {
		memset(lcdon_log, 0, sizeof(struct fto_log));
		memcpy(lcdon_log, event_log, sizeof(struct fto_log));
		lcdon->count++;
		lcdon->lcdon_flag++;
	}

	event->count++;
}

void decon_abd_save_udr(struct abd_protect *abd, unsigned long mif, unsigned long iint, unsigned long disp)
{
	struct decon_device *decon = NULL;
	struct abd_udr *first = NULL;
	struct abd_udr *lcdon = NULL;
	struct abd_udr *event = NULL;

	struct udr_log *first_log = NULL;
	struct udr_log *lcdon_log = NULL;
	struct udr_log *event_log = NULL;

	if (!abd || !abd->init_done)
		return;

	if (!mif | !iint | !disp) {
#if defined(CONFIG_EXYNOS_DPU20)
		mif = cal_dfs_get_rate(ACPM_DVFS_MIF);
		iint = cal_dfs_get_rate(ACPM_DVFS_INT);
		disp = cal_dfs_get_rate(ACPM_DVFS_DISP);
#else
		mif = exynos_devfreq_get_domain_freq(DEVFREQ_MIF);
		iint = exynos_devfreq_get_domain_freq(DEVFREQ_INT);
		disp = exynos_devfreq_get_domain_freq(DEVFREQ_DISP);
#endif
	}

	decon = get_abd_container_of(abd);
	first = &abd->u_first;
	lcdon = &abd->u_lcdon;
	event = &abd->u_event;

	first_log = &first->log[(first->count) % ABD_LOG_MAX];
	lcdon_log = &lcdon->log[(lcdon->count) % ABD_LOG_MAX];
	event_log = &event->log[(event->count) % ABD_LOG_MAX];

	event_log->stamp = local_clock();
	event_log->ktime = ktime_get_real_seconds();
	event_log->mif = mif;
	event_log->iint = iint;
	event_log->disp = disp;
	event_log->aclk = decon->prev_aclk_khz;
	memcpy(event_log->bts, &decon->bts, sizeof(struct decon_bts));

	if (!first->count) {
		first_log->stamp = event_log->stamp;
		first_log->mif = event_log->mif;
		first_log->iint = event_log->iint;
		first_log->disp = event_log->disp;

		memcpy(first_log->bts, event_log->bts, sizeof(struct decon_bts));
		first->count++;
	}

	if (!lcdon->lcdon_flag) {
		lcdon_log->stamp = event_log->stamp;
		lcdon_log->mif = event_log->mif;
		lcdon_log->iint = event_log->iint;
		lcdon_log->disp = event_log->disp;

		memcpy(lcdon_log->bts, event_log->bts, sizeof(struct decon_bts));
		lcdon->count++;
		lcdon->lcdon_flag++;
	}

	event->count++;
}

static void decon_abd_save_pin(struct abd_protect *abd, struct abd_pin_info *pin, struct abd_pin *trace, bool on)
{
	struct decon_device *decon = NULL;
	struct abd_pin *first = NULL;

	struct pin_log *first_log = NULL;
	struct pin_log *trace_log = NULL;

	if (!abd || !abd->init_done)
		return;

	decon = get_abd_container_of(abd);
	first = &pin->p_first;

	first_log = &first->log[(first->count) % ABD_LOG_MAX];
	trace_log = &trace->log[(trace->count) % ABD_LOG_MAX];

	trace_log->stamp = local_clock();
	trace_log->ktime = ktime_get_real_seconds();
	trace_log->level = pin->level;
	trace_log->state = decon->state;
	trace_log->onoff = on;

	if (!first->count) {
		memset(first_log, 0, sizeof(struct pin_log));
		memcpy(first_log, trace_log, sizeof(struct pin_log));
		first->count++;
	}

	trace->count++;
}

static void decon_abd_pin_clear_pending_bit(int irq)
{
	struct irq_desc *desc;

	desc = irq_to_desc(irq);
	if (desc->irq_data.chip->irq_ack) {
		desc->irq_data.chip->irq_ack(&desc->irq_data);
		desc->istate &= ~IRQS_PENDING;
	}
}

static void decon_abd_pin_enable_irq(int irq, unsigned int on)
{
	if (on) {
		decon_abd_pin_clear_pending_bit(irq);
		enable_irq(irq);
	} else {
		decon_abd_pin_clear_pending_bit(irq);
		disable_irq_nosync(irq);
	}
}

static struct abd_pin_info *decon_abd_find_pin_info(struct abd_protect *abd, unsigned int gpio)
{
	unsigned int i = 0;

	for (i = 0; i < ABD_PIN_MAX; i++) {
		if (abd->pin[i].gpio == gpio)
			return &abd->pin[i];
	}

	return NULL;
}

static void _decon_abd_pin_enable(struct abd_protect *abd, struct abd_pin_info *pin, bool on)
{
	struct abd_pin *trace = &pin->p_lcdon;
	struct abd_pin *event = &pin->p_event;
	unsigned int state = 0;

	if (!abd || !abd->init_done || !pin)
		return;

	if (!pin->gpio && !pin->irq)
		return;

	if (pin->enable == on)
		return;

	pin->enable = on;

	pin->level = gpio_get_value(pin->gpio);

	if (pin->level == pin->active_level)
		decon_abd_save_pin(abd, pin, trace, on);

	dbg_info("%s: on: %d, %s(%3d,%d) level: %d, count: %d(event: %d), state: %d, %s\n", __func__,
		on, pin->name, pin->irq, pin->desc->depth, pin->level, trace->count, event->count, state,
		(pin->level == pin->active_level) ? "abnormal" : "normal");

	if (pin->name && !strcmp(pin->name, "pcd"))
		set_frame_bypass(abd, (pin->level == pin->active_level) ? 1 : 0);

	if (pin->irq)
		decon_abd_pin_enable_irq(pin->irq, on);
}

int decon_abd_pin_enable(struct abd_protect *abd, unsigned int gpio, bool on)
{
	struct abd_pin_info *pin = NULL;

	pin = decon_abd_find_pin_info(abd, gpio);
	if (!pin)
		return -EINVAL;

	_decon_abd_pin_enable(abd, pin, on);

	return 0;
}

void decon_abd_enable(struct abd_protect *abd, unsigned int enable)
{
	unsigned int i = 0;

	if (!abd)
		return;

	if (abd->enable == enable)
		dbg_none("%s: already %s\n", __func__, enable ? "enabled" : "disabled");

	if (abd->enable != enable)
		dbg_info("%s: bypass: %d,%d\n", __func__, get_mipi_rw_bypass(abd), get_frame_bypass(abd));

	if (!abd->enable && enable) {	/* off -> on */
		abd->f_lcdon.lcdon_flag = 0;
		abd->u_lcdon.lcdon_flag = 0;
	}

	abd->enable = enable;

	for (i = 0; i < ABD_PIN_MAX; i++)
		_decon_abd_pin_enable(abd, &abd->pin[i], enable);
}

irqreturn_t decon_abd_handler(int irq, void *dev_id)
{
	struct abd_protect *abd = (struct abd_protect *)dev_id;
	struct decon_device *decon = get_abd_container_of(abd);
	struct abd_pin_info *pin = NULL;
	struct abd_pin *trace = NULL;
	struct abd_pin *lcdon = NULL;
	struct adb_pin_handler *pin_handler = NULL;
	unsigned int i = 0, state = 0;

	spin_lock(&abd->slock);

	for (i = 0; i < ABD_PIN_MAX; i++) {
		pin = &abd->pin[i];
		trace = &pin->p_event;
		lcdon = &pin->p_lcdon;
		if (pin && irq == pin->irq)
			break;
	}

	if (i == ABD_PIN_MAX) {
		dbg_info("%s: irq(%d) is not in abd\n", __func__, irq);
		goto exit;
	}

	pin->level = gpio_get_value(pin->gpio);
	state = decon->state;

	decon_abd_save_pin(abd, pin, trace, 1);

	dbg_info("%s: %s(%d) level: %d, count: %d(lcdon: %d), state: %d, %s\n", __func__,
		pin->name, pin->irq, pin->level, trace->count, lcdon->count, state,
		(pin->level == pin->active_level) ? "abnormal" : "normal");

	if (pin->active_level != pin->level)
		goto exit;

	if (i == ABD_PIN_PCD)
		set_frame_bypass(abd, 1);

	list_for_each_entry(pin_handler, &pin->handler_list, node) {
		if (pin_handler && pin_handler->handler)
			pin_handler->handler(irq, pin_handler->dev_id);
	}

exit:
	spin_unlock(&abd->slock);

	return IRQ_HANDLED;
}

int decon_abd_pin_register_handler(struct abd_protect *abd, int irq, irq_handler_t handler, void *dev_id)
{
	struct abd_pin_info *pin = NULL;
	struct adb_pin_handler *pin_handler = NULL, *tmp = NULL;
	unsigned int i = 0;

	if (!irq) {
		dbg_info("%s: irq(%d) invalid\n", __func__, irq);
		return -EINVAL;
	}

	if (!handler) {
		dbg_info("%s: handler invalid\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < ABD_PIN_MAX; i++) {
		pin = &abd->pin[i];
		if (pin && irq == pin->irq) {
			dbg_info("%s: find irq(%d) for %s pin\n", __func__, irq, pin->name);

			list_for_each_entry_safe(pin_handler, tmp, &pin->handler_list, node) {
				WARN(pin_handler->handler == handler && pin_handler->dev_id == dev_id,
					"%s: already registered handler\n", __func__);
			}

			pin_handler = kzalloc(sizeof(struct adb_pin_handler), GFP_KERNEL);
			if (!pin_handler) {
				dbg_info("%s: handler kzalloc fail\n", __func__);
				break;
			}
			pin_handler->handler = handler;
			pin_handler->dev_id = dev_id;
			list_add_tail(&pin_handler->node, &pin->handler_list);

			dbg_info("%s: handler is registered\n", __func__);
			break;
		}
	}

	if (i == ABD_PIN_MAX) {
		dbg_info("%s: irq(%d) is not in abd\n", __func__, irq);
		return -EINVAL;
	}

	return 0;
}

int decon_abd_pin_unregister_handler(struct abd_protect *abd, int irq, irq_handler_t handler, void *dev_id)
{
	struct abd_pin_info *pin = NULL;
	struct adb_pin_handler *pin_handler = NULL, *tmp = NULL;
	unsigned int i = 0;

	if (!irq) {
		dbg_info("%s: irq(%d) invalid\n", __func__, irq);
		return -EINVAL;
	}

	if (!handler) {
		dbg_info("%s: handler invalid\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < ABD_PIN_MAX; i++) {
		pin = &abd->pin[i];
		if (pin && irq == pin->irq) {
			dbg_info("%s: find irq(%d) for %s pin\n", __func__, irq, pin->name);

			list_for_each_entry_safe(pin_handler, tmp, &pin->handler_list, node) {
				if (pin_handler->handler == handler && pin_handler->dev_id == dev_id)
					list_del(&pin->handler_list);
				kfree(pin_handler);
			}

			dbg_info("%s: handler is unregistered\n", __func__);
			break;
		}
	}

	if (i == ABD_PIN_MAX) {
		dbg_info("%s: irq(%d) is not in abd\n", __func__, irq);
		return -EINVAL;
	}

	return 0;
}

static void _decon_abd_print_pin(struct seq_file *m, struct abd_pin *trace)
{
	struct timeval tv;
	struct rtc_time tm;
	struct pin_log *log;
	unsigned int i = 0;

	if (!trace->count)
		return;

	abd_printf(m, "%s total count: %d\n", trace->name, trace->count);
	for (i = 0; i < ABD_LOG_MAX; i++) {
		log = &trace->log[i];
		if (!log->stamp)
			continue;

		tv = ns_to_timeval(log->stamp);
		rtc_time_to_tm(log->ktime, &tm);
		abd_printf(m, "%d-%02d-%02d %02d:%02d:%02d / %lu.%06lu / level: %d onoff: %d state: %d\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
			(unsigned long)tv.tv_sec, tv.tv_usec, log->level, log->onoff, log->state);
	}
}

static void decon_abd_print_pin(struct seq_file *m, struct abd_pin_info *pin)
{
	if (!pin->irq && !pin->gpio)
		return;

	if (!pin->p_first.count)
		return;

	abd_printf(m, "[%s]\n", pin->name);

	_decon_abd_print_pin(m, &pin->p_first);
	_decon_abd_print_pin(m, &pin->p_lcdon);
	_decon_abd_print_pin(m, &pin->p_event);
}

static const char *sync_status_str(int status)
{
	if (status < 0)
		return "error";

	if (status > 0)
		return "signaled";

	return "active";
}

static void decon_abd_print_fto(struct seq_file *m, struct abd_fto *trace)
{
	struct timeval tv;
	struct rtc_time tm;
	struct fto_log *log;
	unsigned int i = 0;

	if (!trace->count)
		return;

	abd_printf(m, "%s total count: %d\n", trace->name, trace->count);
	for (i = 0; i < ABD_LOG_MAX; i++) {
		log = &trace->log[i];
		if (!log->stamp)
			continue;
		tv = ns_to_timeval(log->stamp);
		rtc_time_to_tm(log->ktime, &tm);

#if defined(CONFIG_SOC_EXYNOS7885)
		abd_printf(m, "%d-%02d-%02d %02d:%02d:%02d / %lu.%06lu / winid: %d, %s:%s\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
			(unsigned long)tv.tv_sec, tv.tv_usec, log->winid, log->fence.name, sync_status_str(atomic_read(&log->fence.status)));
#elif defined(CONFIG_SOC_EXYNOS9810)
		abd_printf(m, "%d-%02d-%02d %02d:%02d:%02d / %lu.%06lu / winid: %d, %s\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
			(unsigned long)tv.tv_sec, tv.tv_usec, log->winid, log->fence.name, sync_status_str(fence_get_status(log->fence.fence)));
#else
		abd_printf(m, "%d-%02d-%02d %02d:%02d:%02d / %lu.%06lu / winid: %d, %s\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
			(unsigned long)tv.tv_sec, tv.tv_usec, log->winid, sync_status_str(dma_fence_get_status_locked(&log->fence)));
#endif
	}
}

static void decon_abd_print_udr(struct seq_file *m, struct abd_udr *trace)
{
	struct timeval tv;
	struct rtc_time tm;
	struct udr_log *log;
	struct decon_bts *bts;
	struct bts_decon_info *bts_info;
	unsigned int i = 0, idx;

	if (!trace->count)
		return;

	abd_printf(m, "%s total count: %d\n", trace->name, trace->count);
	for (i = 0; i < ABD_LOG_MAX; i++) {
		log = &trace->log[i];
		if (!log->stamp)
			continue;
		tv = ns_to_timeval(log->stamp);
		rtc_time_to_tm(log->ktime, &tm);
		abd_printf(m, "%d-%02d-%02d %02d:%02d:%02d / %lu.%06lu\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
			(unsigned long)tv.tv_sec, tv.tv_usec);

		bts = (struct decon_bts *)log->bts;
		bts_info = &bts->bts_info;
		abd_printf(m, "MIF(%lu), INT(%lu), DISP(%lu), ACLK(%lu) / total(%u %u), max(%u %u), peak(%u)\n",
				log->mif, log->iint, log->disp, log->aclk,
				bts->prev_total_bw,
				bts->total_bw,
				bts->prev_max_disp_freq,
				bts->max_disp_freq,
				bts->peak);

		for (idx = 0; idx < BTS_DPP_MAX; ++idx) {
			if (!bts_info->dpp[idx].used)
				continue;

#if defined(CONFIG_SOC_EXYNOS7885)
			abd_printf(m, "DPP[%d] (%d) b(%d) s(%4d %4d) d(%4d %4d %4d %4d)\n",
				idx, bts_info->dpp[idx].idma_type, bts_info->dpp[idx].bpp,
				bts_info->dpp[idx].src_w, bts_info->dpp[idx].src_h,
				bts_info->dpp[idx].dst.x1, bts_info->dpp[idx].dst.x2,
				bts_info->dpp[idx].dst.y1, bts_info->dpp[idx].dst.y2);
#else
			abd_printf(m, "DPP[%d] b(%d) s(%4d %4d) d(%4d %4d %4d %4d) r(%d)\n",
				idx, bts_info->dpp[idx].bpp,
				bts_info->dpp[idx].src_w, bts_info->dpp[idx].src_h,
				bts_info->dpp[idx].dst.x1, bts_info->dpp[idx].dst.x2,
				bts_info->dpp[idx].dst.y1, bts_info->dpp[idx].dst.y2,
				bts_info->dpp[idx].rotation);
#endif
		}
	}
}

static void decon_abd_print_ss_log(struct abd_protect *abd, struct seq_file *m)
{
	unsigned int log_max = 200, i, idx;
	struct timeval tv;
	struct decon_device *decon = get_abd_container_of(abd);
	int start = atomic_read(&decon->d.event_log_idx);
	struct dpu_log *log;

	start = (start > log_max) ? start - log_max + 1 : 0;

	for (i = 0; i < log_max; i++) {
		idx = (start + i) % DPU_EVENT_LOG_MAX;
		log = &decon->d.event_log[idx];

		if (!ktime_to_ns(log->time))
			continue;
		tv = ktime_to_timeval(log->time);
		if (i && !(i % 10))
			abd_printf(m, "\n");
		abd_printf(m, "%lu.%06lu %2u, ", (unsigned long)tv.tv_sec, tv.tv_usec, log->type);
	}

	abd_printf(m, "\n");
}

static void decon_abd_print_str(struct seq_file *m, struct abd_str *trace)
{
	unsigned int log_max = ABD_LOG_MAX, i, idx;
	struct timeval tv;
	struct rtc_time tm;
	int start = trace->count - 1;
	struct str_log *log;
	char print_buf[200] = {0, };
	struct seq_file p = {
		.buf = print_buf,
		.size = sizeof(print_buf) - 1,
	};

	if (start < 0)
		return;

	abd_printf(m, "==========_STR_DEBUG_==========\n");

	start = (start > log_max) ? start - log_max + 1 : 0;

	for (i = 0; i < log_max; i++) {
		idx = (start + i) % ABD_LOG_MAX;
		log = &trace->log[idx];

		if (!log->stamp)
			continue;
		tv = ns_to_timeval(log->stamp);
		rtc_time_to_tm(log->ktime, &tm);
		if (i && !(i % 2)) {
			abd_printf(m, "%s\n", p.buf);
			p.count = 0;
			memset(print_buf, 0, sizeof(print_buf));
		}
		seq_printf(&p, "%d-%02d-%02d %02d:%02d:%02d / %lu.%06lu / %-20s ",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
			(unsigned long)tv.tv_sec, tv.tv_usec, log->print);
	}

	abd_printf(m, "%s\n", p.count ? p.buf : "");
}

static void decon_abd_print_bit(struct seq_file *m, struct abd_bit *trace)
{
	struct bit_log *log;
	unsigned int i = 0;

	if (!trace->count)
		return;

	abd_printf(m, "%s total count: %d\n", trace->name, trace->count);
	for (i = 0; i < ABD_LOG_MAX; i++) {
		log = &trace->log[i];
		if (!log->stamp)
			continue;
		_decon_abd_print_bit(m, log);
	}
}

static int decon_abd_show(struct seq_file *m, void *unused)
{
	struct abd_protect *abd = m->private;
	struct decon_device *decon = get_abd_container_of(abd);
	struct dsim_device *dsim = v4l2_get_subdevdata(decon->out_sd[0]);
	unsigned int i = 0;

	abd_printf(m, "==========_DECON_ABD_==========\n");
	abd_printf(m, "bypass: %d,%d, lcdtype: %6X\n", get_frame_bypass(abd), get_mipi_rw_bypass(abd), get_boot_lcdtype());

	for (i = 0; i < ABD_PIN_MAX; i++) {
		if (abd->pin[i].p_first.count) {
			abd_printf(m, "==========_PIN_DEBUG_==========\n");
			break;
		}
	}
	for (i = 0; i < ABD_PIN_MAX; i++)
		decon_abd_print_pin(m, &abd->pin[i]);

	if (abd->b_first.count) {
		abd_printf(m, "==========_BIT_DEBUG_==========\n");
		decon_abd_print_bit(m, &abd->b_first);
		decon_abd_print_bit(m, &abd->b_event);
	}

	if (abd->f_first.count) {
		abd_printf(m, "==========_FTO_DEBUG_==========\n");
		decon_abd_print_fto(m, &abd->f_first);
		decon_abd_print_fto(m, &abd->f_lcdon);
		decon_abd_print_fto(m, &abd->f_event);
	}

	if (abd->u_first.count) {
		abd_printf(m, "==========_UDR_DEBUG_==========\n");
		abd_printf(m, "dsim underrun irq occurs(%d)\n", dsim->total_underrun_cnt);
		decon_abd_print_udr(m, &abd->u_first);
		decon_abd_print_udr(m, &abd->u_lcdon);
		decon_abd_print_udr(m, &abd->u_event);
	}

	decon_abd_print_str(m, &abd->s_event);

	abd_printf(m, "==========_RAM_DEBUG_==========\n");
	decon_abd_print_ss_log(abd, m);

	return 0;
}

static int decon_abd_open(struct inode *inode, struct file *file)
{
	return single_open(file, decon_abd_show, inode->i_private);
}

static const struct file_operations decon_abd_fops = {
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	.open = decon_abd_open,
};

static int decon_abd_reboot_notifier(struct notifier_block *this,
		unsigned long code, void *unused)
{
	struct abd_protect *abd = container_of(this, struct abd_protect, reboot_notifier);
	unsigned int i = 0;
	struct seq_file *m = NULL;

	dbg_info("++ %s: %lu\n",  __func__, code);

	decon_abd_enable(abd, 0);

	abd_printf(m, "==========_DECON_ABD_==========\n");
	abd_printf(m, "bypass: %d,%d, lcdtype: %6X\n", get_frame_bypass(abd), get_mipi_rw_bypass(abd), get_boot_lcdtype());

	for (i = 0; i < ABD_PIN_MAX; i++) {
		if (abd->pin[i].p_first.count) {
			abd_printf(m, "==========_PIN_DEBUG_==========\n");
			break;
		}
	}
	for (i = 0; i < ABD_PIN_MAX; i++)
		decon_abd_print_pin(m, &abd->pin[i]);

	if (abd->b_first.count) {
		abd_printf(m, "==========_BIT_DEBUG_==========\n");
		decon_abd_print_bit(m, &abd->b_first);
		decon_abd_print_bit(m, &abd->b_event);
	}

	if (abd->f_first.count) {
		abd_printf(m, "==========_FTO_DEBUG_==========\n");
		decon_abd_print_fto(m, &abd->f_first);
		decon_abd_print_fto(m, &abd->f_lcdon);
		decon_abd_print_fto(m, &abd->f_event);
	}

	if (abd->u_first.count) {
		abd_printf(m, "==========_UDR_DEBUG_==========\n");
		decon_abd_print_udr(m, &abd->u_first);
		decon_abd_print_udr(m, &abd->u_lcdon);
		decon_abd_print_udr(m, &abd->u_event);
	}

	decon_abd_print_str(m, &abd->s_event);

	dbg_info("-- %s: %lu\n",  __func__, code);

	return NOTIFY_DONE;
}

static int decon_abd_pin_register_function(struct abd_protect *abd, struct abd_pin_info *pin, char *keyword,
		irqreturn_t func(int irq, void *dev_id))
{
	int ret = 0, gpio = 0;
	enum of_gpio_flags flags;
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;
	struct device *dev = NULL;
	unsigned int irqf_type = IRQF_TRIGGER_RISING;
	struct abd_pin *trace = &pin->p_lcdon;
	char *prefix_gpio = "gpio_";
	char dts_name[10] = {0, };

	if (strlen(keyword) + strlen(prefix_gpio) >= sizeof(dts_name)) {
		dbg_info("%s: %s is too log(%zu)\n", __func__, keyword, strlen(keyword));
		goto exit;
	}

	scnprintf(dts_name, sizeof(dts_name), "%s%s", prefix_gpio, keyword);

	pdev = of_find_abd_dt_parent_platform_device();
	dev = &pdev->dev;

	np = of_find_decon_board(pdev ? &pdev->dev : NULL);

	if (!of_find_property(np, dts_name, NULL))
		goto exit;

	gpio = of_get_named_gpio_flags(np, dts_name, 0, &flags);
	if (!gpio_is_valid(gpio)) {
		dbg_info("%s: gpio_is_valid fail, gpio: %s, %d\n", __func__, dts_name, gpio);
		goto exit;
	}

	dbg_info("%s: found %s(%d) success\n", __func__, dts_name, gpio);

	if (gpio_to_irq(gpio) > 0) {
		pin->gpio = gpio;
		pin->irq = gpio_to_irq(gpio);
		pin->desc = irq_to_desc(pin->irq);
	} else {
		dbg_info("%s: gpio_to_irq fail, gpio: %d, irq: %d\n", __func__, gpio, gpio_to_irq(gpio));
		pin->gpio = gpio;
		pin->irq = 0;
		pin->desc = kzalloc(sizeof(struct irq_desc), GFP_KERNEL);
	}

	pin->active_level = !(flags & OF_GPIO_ACTIVE_LOW);
	irqf_type = (flags & OF_GPIO_ACTIVE_LOW) ? IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING;
	dbg_info("%s: %s is active %s%s\n", __func__, keyword, pin->active_level ? "high" : "low",
		(pin->irq) ? ((irqf_type == IRQF_TRIGGER_RISING) ? ", rising" : ", falling") : "");

	pin->name = keyword;
	pin->p_first.name = "first";
	pin->p_lcdon.name = "lcdon";
	pin->p_event.name = "event";

	pin->level = gpio_get_value(pin->gpio);
	if (pin->level == pin->active_level) {
		dbg_info("%s: %s(%d) is already %s(%d)\n", __func__, keyword, pin->gpio,
			(pin->active_level) ? "high" : "low", pin->level);

		decon_abd_save_pin(abd, pin, trace, 1);

		if (pin->name && !strcmp(pin->name, "pcd"))
			set_frame_bypass(abd, 1);
	}

	if (pin->irq) {
		irq_set_irq_type(pin->irq, irqf_type);
		irq_set_status_flags(pin->irq, _IRQ_NOAUTOEN);
		decon_abd_pin_clear_pending_bit(pin->irq);

		if (devm_request_irq(dev, pin->irq, func, irqf_type, keyword, abd)) {
			dbg_info("%s: failed to request irq for %s\n", __func__, keyword);
			/* pin->gpio = 0; */
			pin->irq = 0;
			goto exit;
		}

		INIT_LIST_HEAD(&pin->handler_list);
	}

exit:
	return ret;
}

#if defined(CONFIG_ARCH_EXYNOS)
static int _decon_abd_fb_blank(struct fb_info *info, int blank)
{
	int ret = 0;

	if (!lock_fb_info(info)) {
		dbg_info("%s: fblock is failed\n", __func__);
		return ret;
	}

	dbg_info("+ %s\n", __func__);

	info->flags |= FBINFO_MISC_USEREVENT;
	ret = fb_blank(info, blank);
	info->flags &= ~FBINFO_MISC_USEREVENT;
	unlock_fb_info(info);

	dbg_info("- %s\n", __func__);

	return 0;
}

static int decon_abd_con_set_dummy_blank(struct abd_protect *abd, struct fb_info *fbinfo, unsigned int dummy)
{
	int ret = NOTIFY_DONE;

	if (dummy) {
		fbinfo->fbops->fb_blank = NULL;
		ret = NOTIFY_STOP_MASK;
	} else {
		_decon_abd_pin_enable(abd, &abd->pin[ABD_PIN_CON], 0);
		fbinfo->fbops->fb_blank = abd->fbops.fb_blank;
		set_frame_bypass(abd, 0);
		abd->con_blank = 0;
		ret = NOTIFY_DONE;
	}

	return ret;
}

static void decon_abd_con_prepare_dummy_info(struct abd_protect *abd)
{
	struct fb_info *fbinfo = get_fbinfo(abd);
	struct fb_ops *fbops = fbinfo->fbops;

	memcpy(&abd->fbops, fbops, sizeof(struct fb_ops));
}
#endif

static int decon_abd_con_fb_blank(struct abd_protect *abd)
{
	struct fb_info *fbinfo = get_fbinfo(abd);

	dbg_info("%s\n", __func__);

	set_mipi_rw_bypass(abd, 1);

	_decon_abd_fb_blank(fbinfo, FB_BLANK_POWERDOWN);

	decon_abd_con_set_dummy_blank(abd, fbinfo, 1);

	//_decon_abd_pin_enable(abd, &abd->pin[ABD_PIN_CON], 1);

	return 0;
}

static void decon_abd_con_work(struct work_struct *work)
{
	struct abd_protect *abd = container_of(work, struct abd_protect, con_work);

	dbg_info("%s\n", __func__);

	decon_abd_con_fb_blank(abd);
}

irqreturn_t decon_abd_con_handler(int irq, void *dev_id)
{
	struct abd_protect *abd = (struct abd_protect *)dev_id;

	dbg_info("%s: %d\n", __func__, abd->con_blank);

	if (!abd->con_blank)
		queue_work(abd->con_workqueue, &abd->con_work);

	abd->con_blank = 1;

	return IRQ_HANDLED;
}

static int decon_abd_con_fb_notifier_callback(struct notifier_block *this,
			unsigned long event, void *data)
{
	struct abd_protect *abd = container_of(this, struct abd_protect, con_fb_notifier);
	struct fb_event *evdata = data;
	int fb_blank;

	switch (event) {
	case FB_EARLY_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	fb_blank = *(int *)evdata->data;

	if (evdata->info->node)
		return NOTIFY_DONE;

	if (!abd->init_done) {
		abd->init_done = 1;
		return NOTIFY_DONE;
	}

	if (fb_blank == FB_BLANK_UNBLANK && event == FB_EARLY_EVENT_BLANK) {
		int gpio_active = of_gpio_get_active("gpio_con");

		flush_workqueue(abd->con_workqueue);
		dbg_info("%s: %s\n", __func__, gpio_active ? "disconnected" : "connected");
		if (gpio_active > 0)
			return decon_abd_con_set_dummy_blank(abd, evdata->info, 1);
		else
			return decon_abd_con_set_dummy_blank(abd, evdata->info, 0);
	}

	return NOTIFY_DONE;
}

static int decon_abd_con_pin_register_hanlder(struct abd_protect *abd)
{
	int ret = 0, gpio = 0;
	enum of_gpio_flags flags;
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;
	unsigned int irqf_type = IRQF_TRIGGER_RISING;
	char *prefix_gpio = "gpio_";
	char dts_name[10] = {0, };
	char *keyword = "con";
	struct abd_pin_info pin = {0, };

	if (strlen(keyword) + strlen(prefix_gpio) >= sizeof(dts_name)) {
		dbg_info("%s: %s is too log(%zu)\n", __func__, keyword, strlen(keyword));
		goto exit;
	}

	scnprintf(dts_name, sizeof(dts_name), "%s%s", prefix_gpio, keyword);

	pdev = of_find_abd_dt_parent_platform_device();

	np = of_find_decon_board(pdev ? &pdev->dev : NULL);

	if (!of_find_property(np, dts_name, NULL)) {
		dbg_info("%s: %s not exist\n", __func__, dts_name);
		goto exit;
	}

	gpio = of_get_named_gpio_flags(np, dts_name, 0, &flags);
	if (!gpio_is_valid(gpio)) {
		dbg_info("%s: gpio_is_valid fail, gpio: %s, %d\n", __func__, dts_name, gpio);
		goto exit;
	}

	dbg_info("%s: found %s(%d) success\n", __func__, dts_name, gpio);

	if (gpio_to_irq(gpio) > 0) {
		pin.gpio = gpio;
		pin.irq = gpio_to_irq(gpio);
	} else {
		dbg_info("%s: gpio_to_irq fail, gpio: %d, irq: %d\n", __func__, gpio, gpio_to_irq(gpio));
		pin.gpio = 0;
		pin.irq = 0;
		goto exit;
	}

	pin.active_level = !(flags & OF_GPIO_ACTIVE_LOW);
	irqf_type = (flags & OF_GPIO_ACTIVE_LOW) ? IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING;
	dbg_info("%s: %s is active %s%s\n", __func__, keyword, pin.active_level ? "high" : "low",
		(irqf_type == IRQF_TRIGGER_RISING) ? ", rising" : ", falling");

	pin.level = gpio_get_value(pin.gpio);
	if (pin.level != pin.active_level)
		decon_abd_pin_register_handler(abd, pin.irq, decon_abd_con_handler, abd);

exit:
	return ret;
}

int decon_abd_con_register(struct abd_protect *abd)
{
	struct platform_device *pdev = NULL;
	struct device_node *np = NULL;

	pdev = of_find_abd_dt_parent_platform_device();

	np = of_find_decon_board(pdev ? &pdev->dev : NULL);

	if (!of_find_property(np, "gpio_con", NULL))
		goto exit;

	decon_abd_con_prepare_dummy_info(abd);

	INIT_WORK(&abd->con_work, decon_abd_con_work);

	abd->con_workqueue = create_singlethread_workqueue("abd_conn_workqueue");
	if (!abd->con_workqueue) {
		dbg_info("%s: create_singlethread_workqueue fail\n", __func__);
		goto exit;
	}

	abd->con_fb_notifier.priority = INT_MAX;
	abd->con_fb_notifier.notifier_call = decon_abd_con_fb_notifier_callback;
	decon_register_notifier(&abd->con_fb_notifier);

	decon_abd_con_pin_register_hanlder(abd);

exit:
	return 0;
}

static void decon_pm_wake(struct abd_protect *abd, unsigned int wake_lock)
{
	struct device *fbdev = NULL;

	if (!abd->fbdev)
		return;

	fbdev = abd->fbdev;

	if (abd->wake_lock_enable == wake_lock)
		return;

	if (wake_lock) {
		pm_stay_awake(fbdev);
		dbg_info("%s: pm_stay_awake", __func__);
	} else if (!wake_lock) {
		pm_relax(fbdev);
		dbg_info("%s: pm_relax", __func__);
	}

	abd->wake_lock_enable = wake_lock;
}

static int decon_abd_pin_early_notifier_callback(struct notifier_block *this,
			unsigned long event, void *data)
{
	struct abd_protect *abd = container_of(this, struct abd_protect, pin_early_notifier);
	struct fb_event *evdata = data;
	int fb_blank;

	switch (event) {
	case FB_EARLY_EVENT_BLANK:
	case DECON_EARLY_EVENT_DOZE:
		break;
	default:
		return NOTIFY_DONE;
	}

	fb_blank = *(int *)evdata->data;

	if (evdata->info->node)
		return NOTIFY_DONE;

	if (IS_EARLY(event) && fb_blank == FB_BLANK_POWERDOWN)
		decon_abd_enable(abd, 0);

	if (IS_EARLY(event) && fb_blank == FB_BLANK_UNBLANK)
		decon_pm_wake(abd, 1);

	return NOTIFY_DONE;
}

static int decon_abd_pin_after_notifier_callback(struct notifier_block *this,
			unsigned long event, void *data)
{
	struct abd_protect *abd = container_of(this, struct abd_protect, pin_after_notifier);
	struct fb_event *evdata = data;
	struct abd_pin_info *pin = NULL;
	unsigned int i = 0;
	int fb_blank;

	switch (event) {
	case FB_EVENT_BLANK:
	case DECON_EVENT_DOZE:
		break;
	default:
		return NOTIFY_DONE;
	}

	fb_blank = *(int *)evdata->data;

	if (evdata->info->node)
		return NOTIFY_DONE;

	if (IS_AFTER(event) && fb_blank == FB_BLANK_UNBLANK)
		decon_abd_enable(abd, 1);
	else if (IS_AFTER(event) && fb_blank == FB_BLANK_POWERDOWN) {
		for (i = 0; i < ABD_PIN_MAX; i++) {
			pin = &abd->pin[i];
			if (pin && pin->irq)
				decon_abd_pin_clear_pending_bit(pin->irq);
		}
	}

	if (IS_AFTER(event) && fb_blank == FB_BLANK_POWERDOWN)
		decon_pm_wake(abd, 0);

	return NOTIFY_DONE;
}

static void decon_abd_pin_register(struct abd_protect *abd)
{
	spin_lock_init(&abd->slock);

	decon_abd_pin_register_function(abd, &abd->pin[ABD_PIN_PCD], "pcd", decon_abd_handler);
	decon_abd_pin_register_function(abd, &abd->pin[ABD_PIN_DET], "det", decon_abd_handler);
	decon_abd_pin_register_function(abd, &abd->pin[ABD_PIN_ERR], "err", decon_abd_handler);
	decon_abd_pin_register_function(abd, &abd->pin[ABD_PIN_CON], "con", decon_abd_handler);
	decon_abd_pin_register_function(abd, &abd->pin[ABD_PIN_LOG], "log", decon_abd_handler);

	abd->pin_early_notifier.notifier_call = decon_abd_pin_early_notifier_callback;
	abd->pin_early_notifier.priority = decon_nb_priority_max.priority - 1;
	decon_register_notifier(&abd->pin_early_notifier);

	abd->pin_after_notifier.notifier_call = decon_abd_pin_after_notifier_callback;
	abd->pin_after_notifier.priority = decon_nb_priority_min.priority + 1;
	decon_register_notifier(&abd->pin_after_notifier);
}

static void decon_abd_register(struct abd_protect *abd)
{
	struct decon_device *decon = get_abd_container_of(abd);
	struct dentry *abd_debugfs_root = decon->d.debug_root;
	unsigned int i = 0;

	dbg_info("%s: ++\n", __func__);

	if (!abd_debugfs_root)
		abd_debugfs_root = debugfs_create_dir("panel", NULL);

	abd->debugfs_root = abd_debugfs_root;

	abd->u_first.name = abd->f_first.name = abd->b_first.name = "first";
	abd->u_lcdon.name = abd->f_lcdon.name = "lcdon";
	abd->u_event.name = abd->f_event.name = abd->b_event.name = abd->s_event.name = "event";

	for (i = 0; i < ABD_LOG_MAX; i++) {
		abd->u_first.log[i].bts = kzalloc(sizeof(struct decon_bts), GFP_KERNEL);
		abd->u_lcdon.log[i].bts = kzalloc(sizeof(struct decon_bts), GFP_KERNEL);
		abd->u_event.log[i].bts = kzalloc(sizeof(struct decon_bts), GFP_KERNEL);
	}

	debugfs_create_file("debug", 0444, abd_debugfs_root, abd, &decon_abd_fops);

	abd->reboot_notifier.notifier_call = decon_abd_reboot_notifier;
	register_reboot_notifier(&abd->reboot_notifier);

	dbg_info("%s: -- entity was registered\n", __func__);
}

static int match_dev_name(struct device *dev, const void *data)
{
	const char *keyword = data;

	return dev_name(dev) ? !!strstr(dev_name(dev), keyword) : 0;
}

struct device *find_lcd_class_device(void)
{
	static struct class *p_lcd_class;
	struct lcd_device *new_ld = NULL;
	struct device *dev = NULL;

	if (!p_lcd_class) {
		new_ld = lcd_device_register("dummy_lcd_class_device", NULL, NULL, NULL);
		if (!new_ld)
			return NULL;

		p_lcd_class = new_ld->dev.class;
		lcd_device_unregister(new_ld);
	}

	dev = class_find_device(p_lcd_class, NULL, "panel", match_dev_name);

	return dev;
}

static int __init decon_abd_init(void)
{
	struct decon_device *container = find_container();
	struct abd_protect *abd = NULL;
	struct fb_info *fbinfo = NULL;

	if (!container) {
		dbg_info("find_container fail\n");
		return 0;
	}

	abd = &container->abd;

	decon_abd_register(abd);
	abd->init_done = 1;

	find_lcd_class_device();

	dbg_info("%s: lcdtype: %6X\n", __func__, get_boot_lcdtype());

	if (get_boot_lcdconnected())
		decon_abd_pin_register(abd);
	else
		set_frame_bypass(abd, 1);

	decon_abd_enable(abd, 1);

	if (IS_ENABLED(CONFIG_ARCH_EXYNOS))
		return 0;

	if (IS_ENABLED(CONFIG_MEDIATEK_SOLUTION))
		return 0;

	fbinfo = get_fbinfo(abd);
	abd->fbdev = fbinfo->dev;

	if (abd->fbdev) {
		device_init_wakeup(abd->fbdev, true);
		dbg_info("%s: device_init_wakeup\n", __func__);
	}

	return 0;
}
late_initcall_sync(decon_abd_init);

