/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2017 Samsung Electronics Co., Ltd.
 *
 * Contact: Hyesoo Yu <hyesoo.yu@samsung.com>
 */

#ifndef __EXYNOS_G2D_DEBUG_H_
#define __EXYNOS_G2D_DEBUG_H_

enum debug_level {
	DBG_NO,
	DBG_INFO,
	DBG_PERF,
	DBG_DEBUG,
};

#define g2d_print(level, fmt, args...)	do { } while(0)

#define g2d_info(fmt, args...)	g2d_print(DBG_INFO, fmt, ##args)
#define g2d_perf(fmt, args...)	g2d_print(DBG_PERF, fmt, ##args)

struct regs_info {
	int start;
	int size;
	const char *name;
};

enum g2d_stamp_id {
	G2D_STAMP_STATE_RUNTIME_PM,
	G2D_STAMP_STATE_TASK_RESOURCE,
	G2D_STAMP_STATE_BEGIN,
	G2D_STAMP_STATE_PUSH,
	G2D_STAMP_STATE_INT,
	G2D_STAMP_STATE_DONE,
	G2D_STAMP_STATE_TIMEOUT_FENCE,
	G2D_STAMP_STATE_TIMEOUT_HW,
	G2D_STAMP_STATE_ERR_INT,
	G2D_STAMP_STATE_MMUFAULT,
	G2D_STAMP_STATE_SHUTDOWN,
	G2D_STAMP_STATE_SUSPEND,
	G2D_STAMP_STATE_RESUME,
	G2D_STAMP_STATE_HWFCBUF,
	G2D_STAMP_STATE_PENDING,
	G2D_STAMP_STATE_FENCE,
	G2D_STAMP_STATE_NUM,
};

void g2d_init_debug(struct g2d_device *dev);
void g2d_destroy_debug(struct g2d_device *dev);
void g2d_stamp_task(struct g2d_task *task, u32 stampid, u64 val);
void g2d_dump_info(struct g2d_device *g2d_dev, struct g2d_task *task);
#endif /* __EXYNOS_G2D_HELPER_H_ */
