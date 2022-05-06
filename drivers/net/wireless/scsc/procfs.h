/******************************************************************************
 *
 * Copyright (c) 2012 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/

#ifndef __SLSI_PROCFS_H__
#define __SLSI_PROCFS_H__

#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/errno.h>

struct slsi_dev;
struct slsi_vif;

#ifdef CONFIG_SCSC_WLAN_ANDROID
# ifndef AID_WIFI
#  define AID_WIFI      1010
# endif

#define SLSI_PROCFS_SET_UID_GID(_entry) \
	do { \
		kuid_t proc_kuid = KUIDT_INIT(AID_WIFI); \
		kgid_t proc_kgid = KGIDT_INIT(AID_WIFI); \
		proc_set_user(_entry, proc_kuid, proc_kgid); \
	} while (0)
#else
#define SLSI_PROCFS_SET_UID_GID(entry)
#endif

#define SLSI_PDE_DATA(inode) PDE_DATA(inode)

/* procfs operations */
int slsi_create_proc_dir(struct slsi_dev *sdev);
void slsi_remove_proc_dir(struct slsi_dev *sdev);

int slsi_procfs_open_file_generic(struct inode *inode, struct file *file);

#define SLSI_PROCFS_SEQ_FILE_OPS(name)                                                      \
	static int slsi_procfs_ ## name ## _show(struct seq_file *m, void *v);              \
	static int slsi_procfs_ ## name ## _open(struct inode *inode, struct file *file)    \
	{                                                                                   \
		return single_open(file, slsi_procfs_  ## name ## _show, SLSI_PDE_DATA(inode)); \
	}                                                                                   \
	static const struct file_operations slsi_procfs_ ## name ## _fops = {               \
		.open = slsi_procfs_ ## name ## _open,                                      \
		.read = seq_read,                                                           \
		.llseek = seq_lseek,                                                        \
		.release = single_release,                                                  \
	}

#define SLSI_PROCFS_SEQ_ADD_FILE(_sdev, name, parent, mode) \
	do {                                                \
		struct proc_dir_entry *entry;               \
		entry = proc_create_data(# name, mode, parent, &slsi_procfs_ ## name ## _fops, _sdev); \
		if (!entry) {                               \
			goto err;                           \
		}                                           \
		SLSI_PROCFS_SET_UID_GID(entry);                            \
	} while (0)

#define SLSI_PROCFS_READ_FILE_OPS(name)                                       \
	static ssize_t slsi_procfs_ ## name ## _read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos); \
	static const struct file_operations slsi_procfs_ ## name ## _fops = { \
		.read = slsi_procfs_ ## name ## _read,                        \
		.open = slsi_procfs_open_file_generic,                        \
		.llseek = generic_file_llseek                                 \
	}

#define SLSI_PROCFS_WRITE_FILE_OPS(name)                                       \
	static ssize_t slsi_procfs_ ## name ## _write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos); \
	static const struct file_operations slsi_procfs_ ## name ## _fops = { \
		.write = slsi_procfs_ ## name ## _write,                        \
		.open = slsi_procfs_open_file_generic,                        \
		.llseek = generic_file_llseek                                 \
	}

#define SLSI_PROCFS_RW_FILE_OPS(name)                                               \
	static ssize_t slsi_procfs_ ## name ## _write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos); \
	static ssize_t                      slsi_procfs_ ## name ## _read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos); \
	static const struct file_operations slsi_procfs_ ## name ## _fops = { \
		.read = slsi_procfs_ ## name ## _read,                        \
		.write = slsi_procfs_ ## name ## _write,                      \
		.open = slsi_procfs_open_file_generic,                        \
		.llseek = generic_file_llseek                                 \
	}

#define SLSI_PROCFS_ADD_FILE(_sdev, name, parent, mode)                    \
	do {                                                               \
		struct proc_dir_entry *entry = proc_create_data(# name, mode, parent, &slsi_procfs_ ## name ## _fops, _sdev); \
		SLSI_PROCFS_SET_UID_GID(entry);                            \
	} while (0)
#define SLSI_PROCFS_REMOVE_FILE(name, parent) remove_proc_entry(# name, parent)

void slsi_procfs_inc_node(void);
void slsi_procfs_dec_node(void);

#endif
