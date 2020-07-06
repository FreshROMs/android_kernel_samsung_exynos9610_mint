/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2017 Samsung Electronics Co., Ltd.
 */

#ifndef _G2D_UAPI_PROCESS_H_
#define _G2D_UAPI_PROCESS_H_

#include "g2d_uapi.h"

struct g2d_device;
struct g2d_task;

int g2d_get_userdata(struct g2d_device *g2d_dev, struct g2d_context *ctx,
		     struct g2d_task *task, struct g2d_task_data *data);
void g2d_put_images(struct g2d_device *g2d_dev, struct g2d_task *task);
int g2d_wait_put_user(struct g2d_device *g2d_dev, struct g2d_task *task,
		      struct g2d_task_data __user *uptr, u32 userflag);

#endif /* _G2D_UAPI_PROCESS_H_ */
