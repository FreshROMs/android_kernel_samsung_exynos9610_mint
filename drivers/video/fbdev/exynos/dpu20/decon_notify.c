// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/fb.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/blkdev.h>

#include "decon_notify.h"

#define dbg_none(fmt, ...)		pr_debug(pr_fmt("decon: "fmt), ##__VA_ARGS__)
#define dbg_info(fmt, ...)		pr_info(pr_fmt("decon: "fmt), ##__VA_ARGS__)
#define NSEC_TO_MSEC(ns)		(div_u64(ns, NSEC_PER_MSEC))

#define __XX(a)	#a,
const char *EVENT_NAME[] = { EVENT_LIST };
const char *STATE_NAME[] = { STATE_LIST };
#undef __XX

enum {
	CHAIN_START,
	CHAIN_END,
	CHAIN_MAX,
};

u32 EVENT_NAME_LEN;
u32 STATE_NAME_LEN;
u64 STAMP_TIME[STAMP_MAX][CHAIN_MAX];

static u32 EVENT_TO_STAMP[EVENT_MAX] = {
	[FB_EVENT_BLANK] =		DECON_STAMP_AFTER,
	[FB_EARLY_EVENT_BLANK] =	DECON_STAMP_EARLY,
	[DECON_EVENT_DOZE] =		DECON_STAMP_AFTER,
	[DECON_EARLY_EVENT_DOZE] =	DECON_STAMP_EARLY,
	[DECON_EVENT_FRAME] =		DECON_STAMP_FRAME,
	[DECON_EVENT_FRAME_SEND] =	DECON_STAMP_FRAME_SEND,
	[DECON_EVENT_FRAME_DONE] =	DECON_STAMP_FRAME_DONE,
};

static BLOCKING_NOTIFIER_HEAD(decon_notifier_list);

int decon_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&decon_notifier_list, nb);
}
EXPORT_SYMBOL(decon_register_notifier);

int decon_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&decon_notifier_list, nb);
}
EXPORT_SYMBOL(decon_unregister_notifier);

int decon_notifier_call_chain(unsigned long val, void *v)
{
	int state = 0, ret = 0;
	struct fb_event *evdata = NULL;
	u64 early_delta = 0, blank_delta = 0, after_delta = 0;
	u64 extra_delta = 0, total_delta = 0, frame_delta = 0;
	u64 current_clock = 0;
	u32 current_index = 0, current_first = 0;

	evdata = v;

	if (!evdata || !evdata->info || !evdata->data)
		return NOTIFY_DONE;

	if (evdata->info->node)
		return NOTIFY_DONE;

	state = *(int *)evdata->data;

	if (val >= EVENT_MAX || state >= STATE_MAX) {
		dbg_info("invalid notifier info: %d, %02lx\n", state, val);
		return NOTIFY_DONE;
	}

	current_index = EVENT_TO_STAMP[val] ? EVENT_TO_STAMP[val] : DECON_STAMP_UNKNOWN;

	WARN_ON(current_index == DECON_STAMP_UNKNOWN);

	STAMP_TIME[current_index][CHAIN_START] = current_clock = local_clock();
	STAMP_TIME[DECON_STAMP_BLANK][CHAIN_END] = (val == DECON_EVENT_DOZE) ? current_clock : STAMP_TIME[DECON_STAMP_BLANK][CHAIN_END];

	if (IS_EARLY(val)) {
		dbg_info("decon_notifier: blank_mode: %d, %02lx, + %-*s, %-*s\n",
			state, val, STATE_NAME_LEN, STATE_NAME[state], EVENT_NAME_LEN, EVENT_NAME[val]);
	}

	ret = blocking_notifier_call_chain(&decon_notifier_list, val, v);

	current_first = (STAMP_TIME[DECON_STAMP_AFTER][CHAIN_END] > STAMP_TIME[current_index][CHAIN_END]) ? 1 : 0;

	STAMP_TIME[current_index][CHAIN_END] = current_clock = local_clock();
	STAMP_TIME[DECON_STAMP_BLANK][CHAIN_START] = (val == DECON_EARLY_EVENT_DOZE) ? current_clock : STAMP_TIME[DECON_STAMP_BLANK][CHAIN_START];

	early_delta = STAMP_TIME[DECON_STAMP_EARLY][CHAIN_END] - STAMP_TIME[DECON_STAMP_EARLY][CHAIN_START];
	blank_delta = STAMP_TIME[DECON_STAMP_BLANK][CHAIN_END] - STAMP_TIME[DECON_STAMP_BLANK][CHAIN_START];
	after_delta = STAMP_TIME[DECON_STAMP_AFTER][CHAIN_END] - STAMP_TIME[DECON_STAMP_AFTER][CHAIN_START];

	total_delta = early_delta + blank_delta + after_delta;

	if (IS_AFTER(val)) {
		dbg_info("decon_notifier: blank_mode: %d, %02lx, - %-*s, %-*s, %lld(%lld,%lld,%lld)\n",
			state, val, STATE_NAME_LEN, STATE_NAME[state], EVENT_NAME_LEN, EVENT_NAME[val],
			NSEC_TO_MSEC(total_delta), NSEC_TO_MSEC(early_delta), NSEC_TO_MSEC(blank_delta), NSEC_TO_MSEC(after_delta));
	} else if (current_first && IS_FRAME(val)) {
		extra_delta = current_clock - STAMP_TIME[DECON_STAMP_BLANK][CHAIN_END];
		frame_delta = current_clock - STAMP_TIME[current_index - 1][CHAIN_END];

		total_delta = total_delta + extra_delta;

		dbg_info("decon_notifier: blank_mode: %d, %02lx, * %-*s, %-*s, %lld(%lld)\n",
			state, val, STATE_NAME_LEN, STATE_NAME[state], EVENT_NAME_LEN, EVENT_NAME[val], NSEC_TO_MSEC(frame_delta), NSEC_TO_MSEC(total_delta));
	}

	return ret;
}
EXPORT_SYMBOL(decon_notifier_call_chain);

int decon_simple_notifier_call_chain(unsigned long val, int blank)
{
	struct fb_info *fbinfo = registered_fb[0];
	struct fb_event v = {0, };
	int fb_blank = blank;

	v.info = fbinfo;
	v.data = &fb_blank;

	return decon_notifier_call_chain(val, &v);
}
EXPORT_SYMBOL(decon_simple_notifier_call_chain);

struct notifier_block decon_nb_priority_max = {
	.priority = INT_MAX,
};

struct notifier_block decon_nb_priority_min = {
	.priority = INT_MIN,
};

static int decon_fb_notifier_event(struct notifier_block *this,
	unsigned long val, void *v)
{
	struct fb_event *evdata = NULL;
	int state = 0;

	switch (val) {
	case FB_EARLY_EVENT_BLANK:
	case FB_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	evdata = v;

	if (!evdata || !evdata->info || !evdata->data)
		return NOTIFY_DONE;

	if (evdata->info->node)
		return NOTIFY_DONE;

	state = *(int *)evdata->data;

	if (val >= EVENT_MAX || state >= STATE_MAX) {
		dbg_info("invalid notifier info: %d, %02lx\n", state, val);
		return NOTIFY_DONE;
	}

	decon_notifier_call_chain(val, v);

	return NOTIFY_DONE;
}

static struct notifier_block decon_fb_notifier = {
	.notifier_call = decon_fb_notifier_event,
};

static int decon_fb_notifier_blank_early(struct notifier_block *this,
	unsigned long val, void *v)
{
	struct fb_event *evdata = NULL;
	int state = 0;

	switch (val) {
	case FB_EARLY_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	evdata = v;

	if (!evdata || !evdata->info || !evdata->data)
		return NOTIFY_DONE;

	if (evdata->info->node)
		return NOTIFY_DONE;

	state = *(int *)evdata->data;

	if (val >= EVENT_MAX || state >= STATE_MAX) {
		dbg_info("invalid notifier info: %d, %02lx\n", state, val);
		return NOTIFY_DONE;
	}

	STAMP_TIME[DECON_STAMP_BLANK][CHAIN_START] = local_clock();

	return NOTIFY_DONE;
}

static int decon_fb_notifier_blank_after(struct notifier_block *this,
	unsigned long val, void *v)
{
	struct fb_event *evdata = NULL;
	int state = 0;

	switch (val) {
	case FB_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	evdata = v;

	if (!evdata || !evdata->info || !evdata->data)
		return NOTIFY_DONE;

	if (evdata->info->node)
		return NOTIFY_DONE;

	state = *(int *)evdata->data;

	if (val >= EVENT_MAX || state >= STATE_MAX) {
		dbg_info("invalid notifier info: %d, %02lx\n", state, val);
		return NOTIFY_DONE;
	}

	STAMP_TIME[DECON_STAMP_BLANK][CHAIN_END] = local_clock();

	return NOTIFY_DONE;
}

static struct notifier_block decon_fb_notifier_blank_min = {
	.notifier_call = decon_fb_notifier_blank_early,
	.priority = INT_MIN,
};

static struct notifier_block decon_fb_notifier_blank_max = {
	.notifier_call = decon_fb_notifier_blank_after,
	.priority = INT_MAX,
};

static int __init decon_notifier_init(void)
{
	EVENT_NAME_LEN = EVENT_NAME[FB_EARLY_EVENT_BLANK] ? min_t(size_t, MAX_INPUT, strlen(EVENT_NAME[FB_EARLY_EVENT_BLANK])) : EVENT_NAME_LEN;
	STATE_NAME_LEN = STATE_NAME[FB_BLANK_POWERDOWN] ? min_t(size_t, MAX_INPUT, strlen(STATE_NAME[FB_BLANK_POWERDOWN])) : STATE_NAME_LEN;

	fb_register_client(&decon_fb_notifier_blank_min);
	fb_register_client(&decon_fb_notifier);
	fb_register_client(&decon_fb_notifier_blank_max);

	return 0;
}
core_initcall(decon_notifier_init);

