/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>

#include "vipx-log.h"
#include "vipx-mailbox.h"
#include "vipx-device.h"
#include "vipx-pm.h"
#include "vipx-debug.h"

#define VIPX_DEBUG_LOG_LINE_SIZE		(128)
#define VIPX_DEBUG_LOG_TIME			(10)

static struct vipx_device *debug_device;
int vipx_debug_log_enable;

int vipx_debug_dump_debug_regs(void)
{
	struct vipx_system *sys;

	vipx_enter();
	sys = &debug_device->system;

	sys->ctrl_ops->debug_dump(sys);
	vipx_leave();
	return 0;
}

static int vipx_debug_mem_show(struct seq_file *file, void *unused)
{
	struct vipx_debug *debug;
	struct vipx_memory *mem;

	vipx_enter();
	debug = file->private;
	mem = &debug->system->memory;

	seq_printf(file, "%15s : %zu KB\n",
			mem->fw.name, mem->fw.size / SZ_1K);
	seq_printf(file, "%15s : %zu KB (%zu Bytes used)\n",
			mem->mbox.name, mem->mbox.size / SZ_1K,
			sizeof(struct vipx_mailbox_ctrl));
	seq_printf(file, "%15s : %zu KB\n",
			mem->heap.name, mem->heap.size / SZ_1K);
	seq_printf(file, "%15s : %zu KB\n",
			mem->log.name, mem->log.size / SZ_1K);

	vipx_leave();
	return 0;
}

static int vipx_debug_mem_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, vipx_debug_mem_show, inode->i_private);
}

static ssize_t vipx_debug_mem_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct seq_file *file;
	struct vipx_debug *debug;
	struct vipx_memory *mem;
	struct vipx_pm *pm;
	char buf[128];
	int ret;
	unsigned int fw, mbox, heap, log;
	ssize_t len;

	vipx_enter();
	file = filp->private_data;
	debug = file->private;
	mem = &debug->system->memory;
	pm = &debug->system->pm;

	if (count > sizeof(buf)) {
		vipx_err("[debugfs] writing size(%zd) is larger than buffer\n",
				count);
		goto out;
	}

	len = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (len <= 0) {
		vipx_err("[debugfs] Failed to get user buf(%d)\n", len);
		goto out;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%u %u %u %u\n", &fw, &mbox, &heap, &log);
	if (ret != 4) {
		vipx_err("[debugfs] Failed to get memory size(%d)\n", ret);
		goto out;
	}

	mutex_lock(&pm->lock);
	if (vipx_pm_qos_active(pm)) {
		vipx_warn("[debugfs] size can't be changed (power on)\n");
		mutex_unlock(&pm->lock);
		goto out;
	}

	fw = PAGE_ALIGN(fw * SZ_1K);
	if (fw >= VIPX_CC_DRAM_BIN_SIZE && fw <= VIPX_MEMORY_MAX_SIZE) {
		vipx_info("[debugfs] size of %s is changed (%zu KB -> %u KB)\n",
				mem->fw.name, mem->fw.size / SZ_1K,
				fw / SZ_1K);
		mem->fw.size = fw;
	} else {
		vipx_warn("[debugfs] invalid size %u KB (%s, %u ~ %u)\n",
				fw / SZ_1K, mem->fw.name,
				VIPX_CC_DRAM_BIN_SIZE / SZ_1K,
				VIPX_MEMORY_MAX_SIZE / SZ_1K);
	}

	mbox = PAGE_ALIGN(mbox * SZ_1K);
	if (mbox >= VIPX_MBOX_SIZE && mbox <= VIPX_MEMORY_MAX_SIZE) {
		vipx_info("[debugfs] size of %s is changed (%zu KB -> %u KB)\n",
				mem->mbox.name, mem->mbox.size / SZ_1K,
				mbox / SZ_1K);
		mem->mbox.size = mbox;
	} else {
		vipx_warn("[debugfs] invalid size %u KB (%s, %u ~ %u)\n",
				mbox / SZ_1K, mem->mbox.name,
				VIPX_MBOX_SIZE / SZ_1K,
				VIPX_MEMORY_MAX_SIZE / SZ_1K);
	}

	heap = PAGE_ALIGN(heap * SZ_1K);
	if (heap >= VIPX_HEAP_SIZE && heap <= VIPX_MEMORY_MAX_SIZE) {
		vipx_info("[debugfs] size of %s is changed (%zu KB -> %u KB)\n",
				mem->heap.name, mem->heap.size / SZ_1K,
				heap / SZ_1K);
		mem->heap.size = heap;
	} else {
		vipx_warn("[debugfs] invalid size %u KB (%s, %u ~ %u)\n",
				heap / SZ_1K, mem->heap.name,
				VIPX_HEAP_SIZE / SZ_1K,
				VIPX_MEMORY_MAX_SIZE / SZ_1K);
	}

	log = PAGE_ALIGN(log * SZ_1K);
	if (log >= VIPX_LOG_SIZE && log <= VIPX_MEMORY_MAX_SIZE) {
		vipx_info("[debugfs] size of %s is changed (%zu KB -> %u KB)\n",
				mem->log.name, mem->log.size / SZ_1K,
				log / SZ_1K);
		mem->log.size = log;
	} else {
		vipx_warn("[debugfs] invalid size %u KB (%s, %u ~ %u)\n",
				log / SZ_1K, mem->log.name,
				VIPX_LOG_SIZE / SZ_1K,
				VIPX_MEMORY_MAX_SIZE / SZ_1K);
	}

	mutex_unlock(&pm->lock);

	vipx_leave();
out:
	return count;
}

static const struct file_operations vipx_debug_mem_fops = {
	.open		= vipx_debug_mem_open,
	.read		= seq_read,
	.write		= vipx_debug_mem_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int vipx_debug_dvfs_show(struct seq_file *file, void *unused)
{
#if defined(CONFIG_PM_DEVFREQ)
	struct vipx_debug *debug;
	struct vipx_pm *pm;
	int idx;

	vipx_enter();
	debug = file->private;
	pm = &debug->system->pm;

	mutex_lock(&pm->lock);
	seq_printf(file, "available level count is [L0 - L%d]\n",
			pm->qos_count - 1);
	for (idx = 0; idx < pm->qos_count; ++idx)
		seq_printf(file, "[L%02d] %d\n", idx, pm->qos_table[idx]);

	if (pm->default_qos < 0)
		seq_puts(file, "default: not set\n");
	else
		seq_printf(file, "default: L%d\n", pm->default_qos);

	if (pm->resume_qos < 0)
		seq_puts(file, "resume : not set\n");
	else
		seq_printf(file, "resume : L%d\n", pm->resume_qos);

	if (pm->current_qos < 0)
		seq_puts(file, "current: off\n");
	else
		seq_printf(file, "current: L%d\n", pm->current_qos);

	mutex_unlock(&pm->lock);

	vipx_leave();
	return 0;
#else
	seq_puts(file, "devfreq is not supported\n");
	return 0;
#endif
}

static int vipx_debug_dvfs_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, vipx_debug_dvfs_show, inode->i_private);
}

static ssize_t vipx_debug_dvfs_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct seq_file *file;
	struct vipx_debug *debug;
	struct vipx_pm *pm;
	char buf[30];
	int ret, qos;
	ssize_t len;

	vipx_enter();
	file = filp->private_data;
	debug = file->private;
	pm = &debug->system->pm;

	if (count > sizeof(buf)) {
		vipx_err("[debugfs] writing size(%zd) is larger than buffer\n",
				count);
		goto out;
	}

	len = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (len <= 0) {
		vipx_err("[debugfs] Failed to get user buf(%d)\n", len);
		goto out;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%d\n", &qos);
	if (ret != 1) {
		vipx_err("[debugfs] Failed to get qos value(%d)\n", ret);
		goto out;
	}

	ret = vipx_pm_qos_set_default(pm, qos);
	if (ret) {
		vipx_err("[debugfs] Failed to set default qos(%d)\n", ret);
		goto out;
	} else {
		vipx_info("[debugfs] default qos setting\n");
	}

	vipx_leave();
out:
	return count;
}

static const struct file_operations vipx_debug_dvfs_fops = {
	.open		= vipx_debug_dvfs_open,
	.read		= seq_read,
	.write		= vipx_debug_dvfs_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int vipx_debug_clk_show(struct seq_file *file, void *unused)
{
	struct vipx_debug *debug;
	struct vipx_system *sys;
	struct vipx_pm *pm;
	const struct vipx_clk_ops *ops;
	int count, idx;
	unsigned long freq;
	const char *name;

	vipx_enter();
	debug = file->private;
	sys = debug->system;
	pm = &sys->pm;
	ops = sys->clk_ops;

	mutex_lock(&pm->lock);
	if (vipx_pm_qos_active(pm)) {
		count = ops->get_count(sys);
		for (idx = 0; idx < count; ++idx) {
			freq = ops->get_freq(sys, idx);
			name = ops->get_name(sys, idx);
			seq_printf(file, "%30s(%d) : %3lu.%06lu MHz\n",
					name, idx,
					freq / 1000000, freq % 1000000);
		}
	} else {
		seq_puts(file, "power off\n");
	}
	mutex_unlock(&pm->lock);

	vipx_leave();
	return 0;
}

static int vipx_debug_clk_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, vipx_debug_clk_show, inode->i_private);
}

static const struct file_operations vipx_debug_clk_fops = {
	.open		= vipx_debug_clk_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int vipx_debug_wait_time_show(struct seq_file *file, void *unused)
{
	struct vipx_debug *debug;
	struct vipx_interface *itf;

	vipx_enter();
	debug = file->private;
	itf = &debug->system->interface;

	seq_printf(file, "response wait time %u ms\n", itf->wait_time);

	vipx_leave();
	return 0;
}

static int vipx_debug_wait_time_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, vipx_debug_wait_time_show, inode->i_private);
}

static ssize_t vipx_debug_wait_time_write(struct file *filp,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct seq_file *file;
	struct vipx_debug *debug;
	struct vipx_interface *itf;
	char buf[30];
	int ret, time;
	ssize_t len;

	vipx_enter();
	file = filp->private_data;
	debug = file->private;
	itf = &debug->system->interface;

	if (count > sizeof(buf)) {
		vipx_err("[debugfs] writing size(%zd) is larger than buffer\n",
				count);
		goto out;
	}

	len = simple_write_to_buffer(buf, sizeof(buf), ppos, user_buf, count);
	if (len <= 0) {
		vipx_err("[debugfs] Failed to get user buf(%d)\n", len);
		goto out;
	}

	buf[len] = '\0';

	ret = sscanf(buf, "%d\n", &time);
	if (ret != 1) {
		vipx_err("[debugfs] Failed to get time value(%d)\n", ret);
		goto out;
	}

	vipx_info("[debugfs] wait time is changed form %d ms to %d ms\n",
			itf->wait_time, time);
	itf->wait_time = time;

	vipx_leave();
out:
	return count;
}

static const struct file_operations vipx_debug_wait_time_fops = {
	.open		= vipx_debug_wait_time_open,
	.read		= seq_read,
	.write		= vipx_debug_wait_time_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static int __vipx_debug_write_file(const char *name, void *kva)
{
	int ret;
	mm_segment_t old_fs;
	int fd;
	struct file *fp;
	loff_t pos = 0;
	struct vipx_debug_log_area *area;
	char head[40];
	int write_size;
	int idx;
	char line[134];

	vipx_enter();
	if (!current->fs) {
		vipx_warn("Failed to write %s as fs is invalid\n", name);
		return -ESRCH;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	fd = sys_open(name, O_RDWR | O_CREAT | O_TRUNC, 0640);
	if (fd < 0) {
		ret = fd;
		vipx_err("sys_open(%s) is fail(%d)\n", name, ret);
		goto p_err;
	}

	fp = fget(fd);
	if (!fp) {
		ret = -EFAULT;
		vipx_err("fget(%s) is fail\n", name);
		goto p_err;
	}

	area = kva;
	write_size = snprintf(head, sizeof(head), "%d/%d/%d/%d\n",
			area->front, area->rear,
			area->line_size, area->queue_size);

	vfs_write(fp, head, write_size, &pos);

	for (idx = 0; idx < area->queue_size; ++idx) {
		write_size = snprintf(line, sizeof(line), "[%4d]%s",
				idx, area->queue + (area->line_size * idx));
		if (write_size < 9)
			continue;

		if (line[write_size - 1] != '\n')
			line[write_size - 1] = '\n';

		if (line[write_size] != '\0')
			line[write_size] = '\0';

		vfs_write(fp, line, write_size, &pos);
	}

	fput(fp);
	sys_close(fd);
	set_fs(old_fs);

	vipx_leave();
	return 0;
p_err:
	set_fs(old_fs);
	return ret;
}

int vipx_debug_write_log_binary(void)
{
	int ret;
	struct vipx_system *sys;
	char fname[30];

	vipx_enter();
	sys = &debug_device->system;

	if (!sys->memory.log.kvaddr)
		return -ENOMEM;

	snprintf(fname, sizeof(fname), "%s/%s", VIPX_DEBUG_BIN_PATH,
			"vipx_log.bin");
	ret = __vipx_debug_write_file(fname, sys->memory.log.kvaddr);
	if (!ret)
		vipx_info("%s was created for debugging\n", fname);

	vipx_leave();
	return ret;
}

static bool __vipx_debug_log_valid(struct vipx_debug_log *log)
{
	vipx_check();
	if (!log->area ||
			log->area->front >= log->area->queue_size ||
			log->area->rear >= log->area->queue_size)
		return false;
	else
		return true;
}

static bool __vipx_debug_log_empty(struct vipx_debug_log *log)
{
	vipx_check();
	return (log->area->front == log->area->rear);
}

static void __vipx_debug_log_increase_front(struct vipx_debug_log *log)
{
	vipx_enter();
	log->area->front = (log->area->front + 1) % log->area->queue_size;
	vipx_leave();
}

static void __vipx_debug_log_start(struct vipx_debug *debug)
{
	vipx_enter();
	add_timer(&debug->target_log.timer);
	vipx_leave();
}

static void __vipx_debug_log_stop(struct vipx_debug *debug)
{
	vipx_enter();
	del_timer_sync(&debug->target_log.timer);
	vipx_debug_log_flush(debug);
	vipx_leave();
}

static void __vipx_debug_log_open(struct vipx_debug *debug)
{
	struct vipx_debug_log *log;
	struct vipx_system *sys;

	vipx_enter();
	log = &debug->target_log;
	sys = &debug_device->system;

	log->area = sys->memory.log.kvaddr;
	log->area->front = -1;
	log->area->rear = -1;
	log->area->line_size = VIPX_DEBUG_LOG_LINE_SIZE;
	log->area->queue_size = (sys->memory.log.size - 32) /
		log->area->line_size;
	vipx_leave();
}

static char *__vipx_debug_log_dequeue(struct vipx_debug *debug)
{
	struct vipx_debug_log *log;
	int front;
	char *buf;

	vipx_enter();
	log = &debug->target_log;

	if (__vipx_debug_log_empty(log))
		return NULL;

	if (!__vipx_debug_log_valid(log)) {
		vipx_warn("debug log queue is broken(%d/%d)\n",
				log->area->front, log->area->rear);
		__vipx_debug_log_open(debug);
		return NULL;
	}

	front = (log->area->front + 1) % log->area->queue_size;
	if (front < 0) {
		vipx_warn("debug log queue has invalid value(%d/%d)\n",
				log->area->front, log->area->rear);
		return NULL;
	}

	buf = log->area->queue + (log->area->line_size * front);
	if (buf[log->area->line_size - 2] != '\0')
		buf[log->area->line_size - 2] = '\n';
	buf[log->area->line_size - 1] = '\0';

	vipx_leave();
	return buf;
}

static void vipx_debug_log_print(unsigned long data)
{
	struct vipx_debug *debug;
	struct vipx_debug_log *log;
	char *line;

	vipx_enter();
	debug = (struct vipx_debug *)data;
	log = &debug->target_log;

	while (true) {
		line = __vipx_debug_log_dequeue(debug);
		if (!line)
			break;
		vipx_info("[timer(%4d)] %s",
				(log->area->front + 1) % log->area->queue_size,
				line);
		__vipx_debug_log_increase_front(log);
	}

	mod_timer(&log->timer, jiffies + msecs_to_jiffies(VIPX_DEBUG_LOG_TIME));
	vipx_leave();
}

static void __vipx_debug_log_init(struct vipx_debug *debug)
{
	struct vipx_debug_log *log;

	vipx_enter();
	log = &debug->target_log;

	init_timer(&log->timer);
	log->timer.expires = jiffies + msecs_to_jiffies(VIPX_DEBUG_LOG_TIME);
	log->timer.data = (unsigned long)debug;
	log->timer.function = vipx_debug_log_print;
	vipx_leave();
}

void vipx_debug_log_flush(struct vipx_debug *debug)
{
	struct vipx_debug_log *log;
	char *line;

	vipx_enter();
	log = &debug->target_log;

	while (true) {
		line = __vipx_debug_log_dequeue(debug);
		if (!line)
			break;
		vipx_info("[flush(%4d)] %s",
				(log->area->front + 1) % log->area->queue_size,
				line);
		__vipx_debug_log_increase_front(log);
	}
	vipx_leave();
}

int vipx_debug_start(struct vipx_debug *debug)
{
	vipx_enter();
	__vipx_debug_log_start(debug);
	set_bit(VIPX_DEBUG_STATE_START, &debug->state);
	vipx_leave();
	return 0;
}

int vipx_debug_stop(struct vipx_debug *debug)
{
	vipx_enter();
	clear_bit(VIPX_DEBUG_STATE_START, &debug->state);
	__vipx_debug_log_stop(debug);
	vipx_leave();
	return 0;
}

int vipx_debug_open(struct vipx_debug *debug)
{
	vipx_enter();
	__vipx_debug_log_open(debug);
	vipx_leave();
	return 0;
}

int vipx_debug_close(struct vipx_debug *debug)
{
	vipx_enter();
	vipx_debug_log_flush(debug);
	if (debug->log_bin_enable)
		vipx_debug_write_log_binary();
	vipx_leave();
	return 0;
}

int vipx_debug_probe(struct vipx_device *device)
{
	struct vipx_debug *debug;

	vipx_enter();
	debug_device = device;
	debug = &device->debug;
	debug->system = &device->system;
	debug->state = 0;

	debug->root = debugfs_create_dir("vipx", NULL);
	if (!debug->root) {
		vipx_err("Failed to create debug root file\n");
		goto p_end;
	}

	debug->mem = debugfs_create_file("mem", 0640, debug->root, debug,
			&vipx_debug_mem_fops);
	if (!debug->mem)
		vipx_err("Failed to create mem debugfs file\n");

	debug->log = debugfs_create_u32("log", 0640, debug->root,
			&vipx_debug_log_enable);
	if (!debug->log)
		vipx_err("Failed to create log debugfs file\n");

	debug->log_bin = debugfs_create_u32("log_bin", 0640, debug->root,
			&debug->log_bin_enable);
	if (!debug->log_bin)
		vipx_err("Failed to create log_bin debugfs file\n");

	debug->dvfs = debugfs_create_file("dvfs", 0640, debug->root, debug,
			&vipx_debug_dvfs_fops);
	if (!debug->dvfs)
		vipx_err("Failed to create dvfs debugfs file\n");

	debug->clk = debugfs_create_file("clk", 0640, debug->root, debug,
			&vipx_debug_clk_fops);
	if (!debug->clk)
		vipx_err("Failed to create clk debugfs file\n");

	debug->wait_time = debugfs_create_file("wait_time", 0640, debug->root,
			debug, &vipx_debug_wait_time_fops);
	if (!debug->wait_time)
		vipx_err("Failed to create wait_time debugfs file\n");

	__vipx_debug_log_init(debug);

	vipx_leave();
p_end:
	return 0;
}

void vipx_debug_remove(struct vipx_debug *debug)
{
	debugfs_remove_recursive(debug->root);
}
