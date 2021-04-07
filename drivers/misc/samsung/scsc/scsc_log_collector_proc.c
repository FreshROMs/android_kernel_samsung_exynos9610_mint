/****************************************************************************
 *
 * Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/uaccess.h>
#include <linux/ctype.h>
#include <scsc/scsc_log_collector.h>
#include "scsc_log_collector_proc.h"

static struct proc_dir_entry *procfs_dir;

static int log_collect_procfs_open_file_generic(struct inode *inode, struct file *file)
{
	file->private_data = LOG_COLLECT_PDE_DATA(inode);
	return 0;
}

LOG_COLLECT_PROCFS_RW_FILE_OPS(trigger_collection);

static ssize_t log_collect_procfs_trigger_collection_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char         buf[128];
	int          pos = 0;
	const size_t bufsz = sizeof(buf);

	/* Avoid unused parameter error */
	(void)file;

	pos += scnprintf(buf + pos, bufsz - pos, "%s\n", "OK");

	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}

static ssize_t log_collect_procfs_trigger_collection_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	char val;

	/* check that only one digit is passed */
	if (count != 2)
		pr_err("%s: Incorrect argument length\n", __func__);

	if (copy_from_user(&val, user_buf, 1))
		return -EFAULT;

	if (val == '1') {
		pr_info("%s: Userland has triggered log collection\n", __func__);
		scsc_log_collector_schedule_collection(SCSC_LOG_USER, SCSC_LOG_USER_REASON_PROC);
	} else if (val == '2') {
		pr_info("%s: Dumpstate/dumpsys has triggered log collection\n", __func__);
		scsc_log_collector_schedule_collection(SCSC_LOG_DUMPSTATE, SCSC_LOG_DUMPSTATE_REASON);
	} else {
		pr_err("%s: Incorrect argument\n", __func__);
	}
	return count;
}

static const char *procdir = "driver/scsc_log_collect";

#define LOG_COLLECT_DIRLEN 128

int scsc_log_collect_proc_create(void)
{
	char dir[LOG_COLLECT_DIRLEN];
	struct proc_dir_entry *parent;

	(void)snprintf(dir, sizeof(dir), "%s", procdir);
	parent = proc_mkdir(dir, NULL);
	if (parent) {
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 4, 0))
		parent->data = NULL;
#endif
		procfs_dir = parent;

		LOG_COLLECT_PROCFS_ADD_FILE(NULL, trigger_collection, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	} else {
		pr_err("failed to create /proc dir\n");
		return -EINVAL;
	}

	return 0;
}

void scsc_log_collect_proc_remove(void)
{
	if (procfs_dir) {
		char dir[LOG_COLLECT_DIRLEN];

		LOG_COLLECT_PROCFS_REMOVE_FILE(trigger_collection, procfs_dir);
		(void)snprintf(dir, sizeof(dir), "%s", procdir);
		remove_proc_entry(dir, NULL);
		procfs_dir = NULL;
	}
}
