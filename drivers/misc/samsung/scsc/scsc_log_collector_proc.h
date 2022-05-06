/****************************************************************************
 *
 * Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/*
 * Chip Manager /proc interface
 */
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/seq_file.h>

#ifndef SCSC_LOG_COLLECT_PROC_H
#define SCSC_LOG_COLLECT_PROC_H

#ifndef AID_MX
#define AID_MX  0444
#endif

#define LOG_COLLECT_PDE_DATA(inode) PDE_DATA(inode)

#define LOG_COLLECT_PROCFS_RW_FILE_OPS(name)                                           \
	static ssize_t log_collect_procfs_ ## name ## _write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos); \
	static ssize_t                      log_collect_procfs_ ## name ## _read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos); \
	static const struct file_operations log_collect_procfs_ ## name ## _fops = { \
		.read = log_collect_procfs_ ## name ## _read,                        \
		.write = log_collect_procfs_ ## name ## _write,                      \
		.open = log_collect_procfs_open_file_generic,                     \
		.llseek = generic_file_llseek                                 \
	}


#define LOG_COLLECT_PROCFS_SET_UID_GID(_entry) \
	do { \
		kuid_t proc_kuid = KUIDT_INIT(AID_MX); \
		kgid_t proc_kgid = KGIDT_INIT(AID_MX); \
		proc_set_user(_entry, proc_kuid, proc_kgid); \
	} while (0)

#define LOG_COLLECT_PROCFS_ADD_FILE(_sdev, name, parent, mode)                      \
	do {                                                               \
		struct proc_dir_entry *entry = proc_create_data(# name, mode, parent, &log_collect_procfs_ ## name ## _fops, _sdev); \
		LOG_COLLECT_PROCFS_SET_UID_GID(entry);                            \
	} while (0)

#define LOG_COLLECT_PROCFS_REMOVE_FILE(name, parent) remove_proc_entry(# name, parent)

int scsc_log_collect_proc_create(void);
void scsc_log_collect_proc_remove(void);

#endif /* SCSC_log_collect__PROC_H */
