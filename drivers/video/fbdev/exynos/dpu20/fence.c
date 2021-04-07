/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * DPU fence file for Samsung EXYNOS DPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#if !defined(CONFIG_SUPPORT_LEGACY_FENCE)
#include <linux/dma-fence.h>
#endif
#include <linux/sync_file.h>

#include "decon.h"

char acq_fence_log[ACQ_FENCE_LEN];

#if defined(CONFIG_SUPPORT_LEGACY_FENCE)
/* sync fence related functions */
void decon_create_timeline(struct decon_device *decon, char *name)
{
	decon->timeline = sync_timeline_create(name);
	decon->timeline_max = 0;
}

int decon_get_valid_fd(void)
{
	int fd = 0;
	int fd_idx = 0;
	int unused_fd[FD_TRY_CNT] = {0};

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0)
		return -EINVAL;

	if (fd < VALID_FD_VAL) {
		/*
		 * If fd from get_unused_fd() has value between 0 and 2,
		 * fd is tried to get value again except current fd vlaue.
		 */
		while (fd < VALID_FD_VAL) {
			decon_warn("%s, unvalid fd[%d] is assigned to DECON\n",
					__func__, fd);
			unused_fd[fd_idx++] = fd;
			fd = get_unused_fd_flags(O_CLOEXEC);
			if (fd < 0) {
				decon_err("%s, unvalid fd[%d]\n", __func__,
						fd);
				break;
			}
		}

		while (fd_idx-- > 0) {
			decon_warn("%s, unvalid fd[%d] is released by DECON\n",
					__func__, unused_fd[fd_idx]);
			put_unused_fd(unused_fd[fd_idx]);
		}

		if (fd < 0)
			return -EINVAL;
	}
	return fd;
}

void decon_create_release_fences(struct decon_device *decon,
		struct decon_win_config_data *win_data,
		struct sync_file *sync_file)
{
	int i = 0;

	for (i = 0; i < decon->dt.max_win; i++) {
		int state = win_data->config[i].state;
		int rel_fence = -1;

		if (state == DECON_WIN_STATE_BUFFER) {
			rel_fence = decon_get_valid_fd();
			if (rel_fence < 0) {
				decon_err("%s: failed to get unused fd\n",
						__func__);
				goto err;
			}

			fd_install(rel_fence,
					get_file(sync_file->file));
		}
		win_data->config[i].rel_fence = rel_fence;
	}
	return;
err:
	while (i-- > 0) {
		if (win_data->config[i].state == DECON_WIN_STATE_BUFFER) {
			put_unused_fd(win_data->config[i].rel_fence);
			win_data->config[i].rel_fence = -1;
		}
	}
	return;
}

int decon_create_fence(struct decon_device *decon, struct sync_file **sync_file)
{
	struct sync_pt *pt;
	int fd = -EMFILE;

	decon->timeline_max++;
	pt = sync_pt_create(decon->timeline, sizeof(*pt), decon->timeline_max);
	if (!pt) {
		decon_err("%s: failed to create sync pt\n", __func__);
		goto err;
	}

	*sync_file = sync_file_create(&pt->base);
	fence_put(&pt->base);
	if (!(*sync_file)) {
		decon_err("%s: failed to create sync file\n", __func__);
		goto err;
	}

	fd = decon_get_valid_fd();
	if (fd < 0) {
		decon_err("%s: failed to get unused fd\n", __func__);
		fput((*sync_file)->file);
		goto err;
	}

	return fd;

err:
	decon->timeline_max--;
	return fd;
}

int decon_wait_fence(struct sync_file *sync_file)
{
	int err = sync_file_wait(sync_file, 900);
	if (err >= 0)
		return err;

	if (err < 0)
		decon_warn("error waiting on acquire fence: %d\n", err);
	return err;
}

void decon_signal_fence(struct decon_device *decon)
{
	sync_timeline_signal(decon->timeline, 1);
}
#else	/* dma fence in kernel version 4.14 */
/* sync fence related functions */
void decon_create_timeline(struct decon_device *decon, char *name)
{
	decon->fence.context = dma_fence_context_alloc(1);
	spin_lock_init(&decon->fence.lock);
	strlcpy(decon->fence.name, name, sizeof(decon->fence.name));
}

static int decon_get_valid_fd(void)
{
	int fd = 0;
	int fd_idx = 0;
	int unused_fd[FD_TRY_CNT] = {0};

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0)
		return -EINVAL;

	if (fd < VALID_FD_VAL) {
		/*
		 * If fd from get_unused_fd() has value between 0 and 2,
		 * fd is tried to get value again except current fd vlaue.
		 */
		while (fd < VALID_FD_VAL) {
			decon_warn("%s, unvalid fd[%d] is assigned to DECON\n",
					__func__, fd);
			unused_fd[fd_idx++] = fd;
			fd = get_unused_fd_flags(O_CLOEXEC);
			if (fd < 0) {
				decon_err("%s, unvalid fd[%d]\n", __func__,
						fd);
				break;
			}
		}

		while (fd_idx-- > 0) {
			decon_warn("%s, unvalid fd[%d] is released by DECON\n",
					__func__, unused_fd[fd_idx]);
			put_unused_fd(unused_fd[fd_idx]);
		}

		if (fd < 0)
			return -EINVAL;
	}
	return fd;
}

void decon_create_release_fences(struct decon_device *decon,
		struct decon_win_config_data *win_data,
		struct sync_file *sync_file)
{
	int i = 0;

	for (i = 0; i < decon->dt.max_win; i++) {
		int state = win_data->config[i].state;
		int rel_fence = -1;

		if (state == DECON_WIN_STATE_BUFFER) {
			rel_fence = decon_get_valid_fd();
			if (rel_fence < 0) {
				decon_err("%s: failed to get unused fd\n",
						__func__);
				goto err;
			}

			fd_install(rel_fence,
					get_file(sync_file->file));
		}
		win_data->config[i].rel_fence = rel_fence;
	}
	return;
err:
	while (i-- > 0) {
		if (win_data->config[i].state == DECON_WIN_STATE_BUFFER) {
			put_unused_fd(win_data->config[i].rel_fence);
			win_data->config[i].rel_fence = -1;
		}
	}
	return;
}

static const char *decon_fence_get_driver_name(struct dma_fence *fence)
{
	struct decon_fence *decon_fence;

	decon_fence = container_of(fence->lock, struct decon_fence, lock);
	return decon_fence->name;
}

static bool decon_fence_enable_signaling(struct dma_fence *fence)
{
	/* nothing to do */
	return true;
}

static void decon_fence_value_str(struct dma_fence *fence, char *str, int size)
{
	snprintf(str, size, "%d", fence->seqno);
}

static struct dma_fence_ops decon_fence_ops = {
	.get_driver_name =	decon_fence_get_driver_name,
	.get_timeline_name =	decon_fence_get_driver_name,
	.enable_signaling =	decon_fence_enable_signaling,
	.wait =			dma_fence_default_wait,
	.fence_value_str =	decon_fence_value_str,
};

int decon_create_fence(struct decon_device *decon, struct sync_file **sync_file)
{
	struct dma_fence *fence;
	int fd = -EMFILE;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (!fence)
		return -ENOMEM;

	dma_fence_init(fence, &decon_fence_ops, &decon->fence.lock,
		   decon->fence.context,
		   atomic_inc_return(&decon->fence.timeline));

	*sync_file = sync_file_create(fence);
	dma_fence_put(fence);
	if (!(*sync_file)) {
		decon_err("%s: failed to create sync file\n", __func__);
		return -ENOMEM;
	}

	fd = decon_get_valid_fd();
	if (fd < 0) {
		decon_err("%s: failed to get unused fd\n", __func__);
		fput((*sync_file)->file);
	}

	return fd;
}

int decon_wait_fence(struct dma_fence *fence)
{
	int err = 0;

	snprintf(acq_fence_log, ACQ_FENCE_LEN, "%p:%s",
			fence, fence->ops->get_driver_name(fence));

	err = dma_fence_wait_timeout(fence, false, 900);
	if (err < 0)
		decon_warn("%s: error waiting on acquire fence: %d\n", acq_fence_log, err);

	return err;
}

void decon_signal_fence(struct dma_fence *fence)
{
	if (dma_fence_signal(fence)) {
		if (fence)
			decon_warn("%s: fence[%p] #%d signal failed\n", __func__,
					fence, fence->seqno);
		else
			decon_warn("%s: fence is null\n", __func__);
	}
}
#endif
