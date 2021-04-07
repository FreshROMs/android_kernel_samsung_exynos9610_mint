/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/iommu.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include "chub_log.h"
#include "chub_dbg.h"
#include "chub_ipc.h"

#ifdef CONFIG_CONTEXTHUB_DEBUG
#define SIZE_OF_BUFFER (SZ_512K + SZ_128K)
#else
#define SIZE_OF_BUFFER (SZ_128K)
#endif

#undef CHUB_LOG_DUMP_SUPPORT
#define S_IRWUG (0660)
#define DEFAULT_FLUSH_MS (1000)

static u32 log_auto_save;
static struct dentry *dbg_root_dir __read_mostly;
static LIST_HEAD(log_list_head);
static struct log_buffer_info *print_info;
u32 auto_log_flush_ms;

static void log_memcpy(struct log_buffer_info *info,
		       struct log_kernel_buffer *kernel_buffer,
		       const char *src, size_t size)
{
	mm_segment_t old_fs;

	size_t left_size = SIZE_OF_BUFFER - kernel_buffer->index;

	dev_dbg(info->dev, "%s(%zu)\n", __func__, size);
	if (size > SIZE_OF_BUFFER) {
		dev_warn(info->dev,
			 "flush size (%zu, %zu) is bigger than kernel buffer size (%d)",
			 size, left_size, SIZE_OF_BUFFER);
		size = SIZE_OF_BUFFER;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if(log_auto_save) {
		if (likely(info->file_created)) {
		        info->filp = filp_open(info->save_file_name, O_RDWR | O_APPEND | O_CREAT, S_IRWUG);
		        dev_info(info->dev, "appended to %s\n", info->save_file_name);
		} else {
		        info->filp = filp_open(info->save_file_name, O_RDWR | O_TRUNC | O_CREAT, S_IRWUG);
		        info->file_created = true;
		        dev_info(info->dev, "created %s\n", info->save_file_name);
		}

		if (IS_ERR(info->filp)) {
		        dev_warn(info->dev, "%s: saving log fail\n", __func__);
		        goto out;
		}
	}

	if (left_size < size) {
		if (info->sram_log_buffer)
			memcpy_fromio(kernel_buffer->buffer + kernel_buffer->index, src, left_size);
		else
			memcpy(kernel_buffer->buffer + kernel_buffer->index, src, left_size);

		if (log_auto_save) {
			vfs_write(info->filp, kernel_buffer->buffer + kernel_buffer->index, left_size, &info->filp->f_pos);
			vfs_fsync(info->filp, 0);
		}
		src += left_size;
		size -= left_size;

		kernel_buffer->index = 0;
		kernel_buffer->wrap = true;
	}

	if (info->sram_log_buffer)
		memcpy_fromio(kernel_buffer->buffer + kernel_buffer->index, src, size);
	else
		memcpy(kernel_buffer->buffer + kernel_buffer->index, src, size);

	if (log_auto_save) {
		vfs_write(info->filp,
			  kernel_buffer->buffer + kernel_buffer->index, size, &info->filp->f_pos);
		vfs_fsync(info->filp, 0);
		filp_close(info->filp, NULL);
	}

	kernel_buffer->index += size;

out:
	set_fs(old_fs);
}

void log_flush(struct log_buffer_info *info)
{
	struct LOG_BUFFER *buffer = info->log_buffer;
	struct log_kernel_buffer *kernel_buffer = &info->kernel_buffer;
	unsigned int index_writer = buffer->index_writer;

	/* check logbuf index dueto sram corruption */
	if ((buffer->index_reader >= ipc_get_offset(IPC_REG_LOG))
		|| (buffer->index_writer >= ipc_get_offset(IPC_REG_LOG))) {
		dev_err(info->dev, "%s(%d): offset is corrupted. index_writer=%u, index_reader=%u, size=%u-%u\n",
			__func__, info->id, buffer->index_writer, buffer->index_reader, buffer->size,
			ipc_get_offset(IPC_REG_LOG));

		return;
	}

	if (buffer->index_reader == index_writer)
		return;

	dev_dbg(info->dev,
		"%s(%d): index_writer=%u, index_reader=%u, size=%u\n", __func__,
		info->id, index_writer, buffer->index_reader, buffer->size);

	mutex_lock(&info->lock);

	if (buffer->index_reader > index_writer) {
		log_memcpy(info, kernel_buffer,
			   buffer->buffer + buffer->index_reader,
			   buffer->size - buffer->index_reader);
		buffer->index_reader = 0;
	}
	log_memcpy(info, kernel_buffer,
		   buffer->buffer + buffer->index_reader,
		   index_writer - buffer->index_reader);
	buffer->index_reader = index_writer;

	wmb();
	mutex_unlock(&info->lock);

	kernel_buffer->updated = true;
	wake_up_interruptible(&kernel_buffer->wq);
}

static void log_flush_all_work_func(struct work_struct *work);
static DECLARE_DEFERRABLE_WORK(log_flush_all_work, log_flush_all_work_func);

static void log_flush_all(void)
{
	struct log_buffer_info *info;

	list_for_each_entry(info, &log_list_head, list) {
		if (!info) {
			pr_warn("%s: fails get info\n", __func__);
			return;
		}
		log_flush(info);
	}
}

static void log_flush_all_work_func(struct work_struct *work)
{
	log_flush_all();

	if (auto_log_flush_ms)
		schedule_delayed_work(&log_flush_all_work,
				      msecs_to_jiffies(auto_log_flush_ms));
}

void log_schedule_flush_all(void)
{
	schedule_delayed_work(&log_flush_all_work, msecs_to_jiffies(3000));
}

static int log_file_open(struct inode *inode, struct file *file)
{
	struct log_buffer_info *info = inode->i_private;

	dev_dbg(info->dev, "%s\n", __func__);

	file->private_data = inode->i_private;
	info->log_file_index = -1;

	return 0;
}

static ssize_t log_file_read(struct file *file, char __user *buf, size_t count,
			     loff_t *ppos)
{
	struct log_buffer_info *info = file->private_data;
	struct log_kernel_buffer *kernel_buffer = &info->kernel_buffer;
	size_t end, size;
	bool first = (info->log_file_index < 0);
	int result;

	dev_dbg(info->dev, "%s(%zu, %lld)\n", __func__, count, *ppos);

	mutex_lock(&info->lock);

	if (info->log_file_index < 0) {
		info->log_file_index =
		    likely(kernel_buffer->wrap) ? kernel_buffer->index : 0;
	}

	do {
		end = ((info->log_file_index < kernel_buffer->index) ||
		       ((info->log_file_index == kernel_buffer->index) &&
			!first)) ? kernel_buffer->index : SIZE_OF_BUFFER;
		size = min(end - info->log_file_index, count);
		if (size == 0) {
			mutex_unlock(&info->lock);
			if (file->f_flags & O_NONBLOCK) {
				dev_dbg(info->dev, "non block\n");
				return -EAGAIN;
			}
			kernel_buffer->updated = false;

			result = wait_event_interruptible(kernel_buffer->wq,
				kernel_buffer->updated);
			if (result != 0) {
				dev_dbg(info->dev, "interrupted\n");
				return result;
			}
			mutex_lock(&info->lock);
		}
	} while (size == 0);

	dev_dbg(info->dev, "start=%zd, end=%zd size=%zd\n",
		info->log_file_index, end, size);
	if (copy_to_user
	    (buf, kernel_buffer->buffer + info->log_file_index, size)) {
		mutex_unlock(&info->lock);
		return -EFAULT;
	}

	info->log_file_index += size;
	if (info->log_file_index >= SIZE_OF_BUFFER)
		info->log_file_index = 0;

	mutex_unlock(&info->lock);

	dev_dbg(info->dev, "%s: size = %zd\n", __func__, size);

	return size;
}

static unsigned int log_file_poll(struct file *file, poll_table *wait)
{
	struct log_buffer_info *info = file->private_data;
	struct log_kernel_buffer *kernel_buffer = &info->kernel_buffer;

	dev_dbg(info->dev, "%s\n", __func__);

	poll_wait(file, &kernel_buffer->wq, wait);
	return POLLIN | POLLRDNORM;
}

static const struct file_operations log_fops = {
	.open = log_file_open,
	.read = log_file_read,
	.poll = log_file_poll,
	.llseek = generic_file_llseek,
	.owner = THIS_MODULE,
};

static struct dentry *chub_dbg_get_root_dir(void)
{
	if (!dbg_root_dir)
		dbg_root_dir = debugfs_create_dir("nanohub", NULL);

	return dbg_root_dir;
}

#ifdef CHUB_LOG_DUMP_SUPPORT
static void chub_log_auto_save_open(struct log_buffer_info *info)
{
	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);

	/* close previous */
	if (info->filp && !IS_ERR(info->filp)) {
		dev_info(info->dev, "%s closing previous file %p\n", __func__, info->filp);
		filp_close(info->filp, current->files);
		}

	info->filp =
	    filp_open(info->save_file_name, O_RDWR | O_TRUNC | O_CREAT,
		      S_IRWUG);

	dev_info(info->dev, "%s created\n", info->save_file_name);

	if (IS_ERR(info->filp))
		dev_warn(info->dev, "%s: saving log fail\n", __func__);

	set_fs(old_fs);
}

static void chub_log_auto_save_ctrl(struct log_buffer_info *info, u32 event)
{
	if (event) {
		/* set file name */
		snprintf(info->save_file_name, sizeof(info->save_file_name),
			 "%s/nano-%02d-00-%06u.log", CHUB_DBG_DIR, info->id,
			 (u32)(sched_clock() / NSEC_PER_SEC));
		chub_log_auto_save_open(info);
		log_auto_save = 1;
	} else {
		log_auto_save = 0;
		info->filp = NULL;
	}

	pr_info("%s: %s, %d, %p\n", __func__, info->save_file_name,
		log_auto_save, info->filp);
}

static ssize_t chub_log_save_show(struct device *kobj,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", log_auto_save);
}

static ssize_t chub_log_save_save(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	long event;
	int err;

	/* auto log_save */
	err = kstrtol(&buf[0], 10, &event);

	if (!err) {
		struct log_buffer_info *info;

		list_for_each_entry(info, &log_list_head, list)
			if (info->support_log_save) /* sram can support it */
				chub_log_auto_save_ctrl(info, event);

		/* set log_flush to save log */
		if (!auto_log_flush_ms) {
			log_schedule_flush_all();
			auto_log_flush_ms = DEFAULT_FLUSH_MS;
			dev_dbg(dev, "%s: set log_flush time(% dms) for log_save\n",
				auto_log_flush_ms);
		}
		return count;
	} else {
		return 0;
	}
}
#endif
#define TMP_BUFFER_SIZE (1000)

#if defined(CONFIG_CONTEXTHUB_DEBUG)
static void log_dump(struct log_buffer_info *info, int err)
{
	struct file *filp;
	mm_segment_t old_fs;
	char save_file_name[64];
	struct LOG_BUFFER *buffer = info->log_buffer;
	u32 wrap_index = buffer->index_writer;

	/* check logbuf index dueto sram corruption */
	if ((buffer->index_reader >= ipc_get_offset(IPC_REG_LOG))
		|| (buffer->index_writer >= ipc_get_offset(IPC_REG_LOG))) {
		dev_err(info->dev, "%s(%d): offset is corrupted. index_writer=%u, index_reader=%u, size=%u-%u\n",
			__func__, info->id, buffer->index_writer, buffer->index_reader, buffer->size,
			ipc_get_offset(IPC_REG_LOG));
		return;
	}

	snprintf(save_file_name, sizeof(save_file_name),
		 "%s/nano-%02d-%02d-%06u.log", CHUB_DBG_DIR,
		 info->id, err, (u32)(sched_clock() / NSEC_PER_SEC));

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	filp = filp_open(save_file_name, O_RDWR | O_TRUNC | O_CREAT, S_IRWUG);
	if (IS_ERR(filp)) {
		dev_warn(info->dev, "%s: fails filp:%p\n", __func__, filp);
		goto out;
	}

	if (info->sram_log_buffer) {
		int i;
		int size;
		bool wrap = false;
		char tmp_buffer[TMP_BUFFER_SIZE];
		u32 start_index = wrap_index;
		int bottom = 0;

		/* dump sram-log buffer to fs (eq ~ eq + logbuf_size) */
		dev_dbg(info->dev, "%s: logbuf:%p, eq:%d, dq:%d, size:%d, loop:%d\n", __func__,
			(void *)buffer, wrap_index,	buffer->index_reader, buffer->size,
			(buffer->size / TMP_BUFFER_SIZE) + 1);
		for (i = 0; i < (buffer->size / TMP_BUFFER_SIZE) + 1;
		     i++, start_index += TMP_BUFFER_SIZE) {
			if (start_index + TMP_BUFFER_SIZE > buffer->size) {
				size = buffer->size - start_index;
				wrap = true;
				bottom = 1;
			} else if (bottom && (wrap_index - start_index < TMP_BUFFER_SIZE))	{
				size = wrap_index - start_index;
			} else {
				size = TMP_BUFFER_SIZE;
			}
			memcpy_fromio(tmp_buffer, buffer->buffer + start_index, size);
			vfs_write(filp, tmp_buffer, size, &filp->f_pos);
			if (wrap) {
				wrap = false;
				start_index = 0;
			}
		}
	} else {
		vfs_write(filp, buffer->buffer + wrap_index, buffer->size - wrap_index,
			  &filp->f_pos);
		vfs_write(filp, buffer->buffer, wrap_index, &filp->f_pos);
	}
	dev_dbg(info->dev, "%s is created\n", save_file_name);

	vfs_fsync(filp, 0);
	filp_close(filp, NULL);

out:
	set_fs(old_fs);
}

void log_dump_all(int err)
{
	struct log_buffer_info *info;

	list_for_each_entry(info, &log_list_head, list)
		log_dump(info, err);
}
#endif

static ssize_t chub_log_flush_show(struct device *kobj,
				   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", auto_log_flush_ms);
}

static ssize_t chub_log_flush_save(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	long event;
	int err;

	err = kstrtol(&buf[0], 10, &event);
	if (!err) {
		if (!auto_log_flush_ms) {
			if (!err) {
				log_flush_all();
			} else {
				pr_err("%s: fails to flush log\n", __func__);
			}
		}
#ifdef CHUB_LOG_DUMP_SUPPORT
		/* update log_flush time */
		auto_log_flush_ms = event * 1000;
#endif
		if (auto_log_flush_ms)
			dev_dbg(dev, "%s is flushed every %d ms.\n", auto_log_flush_ms);
		return count;
	} else {
		return 0;
	}

	return count;
}

static ssize_t chub_dump_log_save(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	log_dump_all(0);
	return count;
}

static struct device_attribute attributes[] = {
#ifdef CHUB_LOG_DUMP_SUPPORT
	/* enable auto-save with flush_log */
	__ATTR(save_log, 0664, chub_log_save_show, chub_log_save_save),
#endif
	/* flush sram-logbuf to dram */
	__ATTR(flush_log, 0664, chub_log_flush_show, chub_log_flush_save),
	/* dump sram-logbuf to file */
	__ATTR(dump_log, 0220, NULL, chub_dump_log_save)
};

struct log_buffer_info *log_register_buffer(struct device *dev, int id,
					    struct LOG_BUFFER *buffer,
					    char *name, bool sram)
{
	struct log_buffer_info *info = vmalloc(sizeof(*info));
	int i;
	int ret;

	if (!info)
		return NULL;

	mutex_init(&info->lock);
	info->id = id;
	info->file_created = false;
	info->kernel_buffer.buffer = vzalloc(SIZE_OF_BUFFER);
	info->kernel_buffer.index = 0;
	info->kernel_buffer.index_reader = 0;
	info->kernel_buffer.index_writer = 0;
	info->kernel_buffer.wrap = false;
	init_waitqueue_head(&info->kernel_buffer.wq);
	info->dev = dev;
	info->log_buffer = buffer;

	/* HACK: clang make error
	buffer->index_reader = 0;
	buffer->index_writer = 0;
	*/
	info->save_file_name[0] = '\0';
	info->filp = NULL;

	dev_info(dev, "%s with %p buffer size %d. %p kernel buffer size %d\n",
		 __func__, buffer->buffer, buffer->size,
		 info->kernel_buffer.buffer, SIZE_OF_BUFFER);

	debugfs_create_file(name, S_IRWUG, chub_dbg_get_root_dir(), info,
			    &log_fops);

	list_add_tail(&info->list, &log_list_head);

	if (sram) {
		info->sram_log_buffer = true;
		info->support_log_save = true;

		/* add device files */
		for (i = 0, ret = 0; i < ARRAY_SIZE(attributes); i++) {
			ret = device_create_file(dev, &attributes[i]);
			if (ret)
				dev_warn(dev, "Failed to create file: %s\n",
					 attributes[i].attr.name);
		}
	} else {
		print_info = info;
		info->sram_log_buffer = false;
		info->support_log_save = false;
	}

	return info;
}

void log_printf(const char *format, ...)
{
	struct LOG_BUFFER *buffer;
	int size;
	va_list args;

	if (print_info) {
		char tmp_buf[512];
		char *buffer_index = tmp_buf;

		buffer = print_info->log_buffer;

		va_start(args, format);
		size = vsprintf(tmp_buf, format, args);
		va_end(args);

		size++;
		if (buffer->index_writer + size > buffer->size) {
			int left_size = buffer->size - buffer->index_writer;

			memcpy(&buffer->buffer[buffer->index_writer],
			       buffer_index, left_size);
			buffer->index_writer = 0;
			buffer_index += left_size;
		}
		memcpy(&buffer->buffer[buffer->index_writer], buffer_index,
		       size - (buffer_index - tmp_buf));
		buffer->index_writer += size - (buffer_index - tmp_buf);

	}
}

static int __init log_late_initcall(void)
{
	debugfs_create_u32("log_auto_save", S_IRWUG, chub_dbg_get_root_dir(),
			   &log_auto_save);
	return 0;
}

late_initcall(log_late_initcall);
