/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/*
 * Chip Manager /proc interface
 */
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/seq_file.h>

#ifndef SCSC_MIF_PROC_H
#define SCSC_MIF_PROC_H

#ifndef AID_MX
#define AID_MX  0444
#endif

#define MIF_PDE_DATA(inode) PDE_DATA(inode)

#define MIF_PROCFS_SEQ_FILE_OPS(name)                                                      \
	static int mifprocfs_ ## name ## _show(struct seq_file *m, void *v);              \
	static int mifprocfs_ ## name ## _open(struct inode *inode, struct file *file)    \
	{                                                                                   \
		return single_open(file, mifprocfs_  ## name ## _show, MIF_PDE_DATA(inode)); \
	}                                                                                   \
	static const struct file_operations mifprocfs_ ## name ## _fops = {               \
		.open = mifprocfs_ ## name ## _open,                                      \
		.read = seq_read,                                                           \
		.llseek = seq_lseek,                                                        \
		.release = single_release,                                                  \
	}

#define MIF_PROCFS_SEQ_ADD_FILE(_sdev, name, parent, mode) \
	do {                                                \
		struct proc_dir_entry *entry;               \
		entry = proc_create_data(# name, mode, parent, &mifprocfs_ ## name ## _fops, _sdev); \
		if (!entry) {                               \
			break;                              \
		}                                           \
		MIF_PROCFS_SET_UID_GID(entry);                            \
	} while (0)

#define MIF_PROCFS_RW_FILE_OPS(name)                                           \
	static ssize_t mifprocfs_ ## name ## _write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos); \
	static ssize_t                      mifprocfs_ ## name ## _read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos); \
	static const struct file_operations mifprocfs_ ## name ## _fops = { \
		.read = mifprocfs_ ## name ## _read,                        \
		.write = mifprocfs_ ## name ## _write,                      \
		.open = mifprocfs_open_file_generic,                     \
		.llseek = generic_file_llseek                                 \
	}


#define MIF_PROCFS_RO_FILE_OPS(name)                                           \
	static ssize_t                      mifprocfs_ ## name ## _read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos); \
	static const struct file_operations mifprocfs_ ## name ## _fops = { \
		.read = mifprocfs_ ## name ## _read,                        \
		.open = mifprocfs_open_file_generic,                     \
		.llseek = generic_file_llseek                                 \
	}

#define MIF_PROCFS_SET_UID_GID(_entry) \
	do { \
		kuid_t proc_kuid = KUIDT_INIT(AID_MX); \
		kgid_t proc_kgid = KGIDT_INIT(AID_MX); \
		proc_set_user(_entry, proc_kuid, proc_kgid); \
	} while (0)

#define MIF_PROCFS_ADD_FILE(_sdev, name, parent, mode)                      \
	do {                                                               \
		struct proc_dir_entry *entry = proc_create_data(# name, mode, parent, &mifprocfs_ ## name ## _fops, _sdev); \
		MIF_PROCFS_SET_UID_GID(entry);                            \
	} while (0)

#define MIF_PROCFS_REMOVE_FILE(name, parent) remove_proc_entry(# name, parent)

struct scsc_mif_abs;
struct miframman;

int mifproc_create_proc_dir(struct scsc_mif_abs *mif);
void mifproc_remove_proc_dir(void);
int mifproc_create_ramman_proc_dir(struct miframman *miframman);
void mifproc_remove_ramman_proc_dir(struct miframman *miframman);

struct mifproc {
};
#endif /* SCSC_mif_PROC_H */
