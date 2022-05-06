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

#include "pcie_mif.h"

#ifndef SCSC_PCIE_PROC_H
#define SCSC_PCIE_PROC_H

#ifndef AID_WIFI
#define AID_WIFI        0444
#endif

#define PCIE_PDE_DATA(inode) PDE_DATA(inode)

#define PCIE_PROCFS_SEQ_FILE_OPS(name)                                                      \
	static int pcie_procfs_ ## name ## _show(struct seq_file *m, void *v);              \
	static int pcie_procfs_ ## name ## _open(struct inode *inode, struct file *file)    \
	{                                                                                   \
		return single_open(file, pcie_procfs_  ## name ## _show, PCIE_PDE_DATA(inode)); \
	}                                                                                   \
	static const struct file_operations pcie_procfs_ ## name ## _fops = {               \
		.open = pcie_procfs_ ## name ## _open,                                      \
		.read = seq_read,                                                           \
		.llseek = seq_lseek,                                                        \
		.release = single_release,                                                  \
	}

#define PCIE_PROCFS_SEQ_ADD_FILE(_sdev, name, parent, mode) \
	do {                                                \
		struct proc_dir_entry *entry;               \
		entry = proc_create_data(# name, mode, parent, &pcie_procfs_ ## name ## _fops, _sdev); \
		if (!entry) {                               \
			goto err;                           \
		}                                           \
		PCIE_PROCFS_SET_UID_GID(entry);                            \
	} while (0)

#define PCIE_PROCFS_RW_FILE_OPS(name)                                           \
	static ssize_t pcie_procfs_ ## name ## _write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos); \
	static ssize_t                      pcie_procfs_ ## name ## _read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos); \
	static const struct file_operations pcie_procfs_ ## name ## _fops = { \
		.read = pcie_procfs_ ## name ## _read,                        \
		.write = pcie_procfs_ ## name ## _write,                      \
		.open = pcie_procfs_open_file_generic,                     \
		.llseek = generic_file_llseek                                 \
	}


#define PCIE_PROCFS_SET_UID_GID(_entry) \
	do { \
		kuid_t proc_kuid = KUIDT_INIT(AID_WIFI); \
		kgid_t proc_kgid = KGIDT_INIT(AID_WIFI); \
		proc_set_user(_entry, proc_kuid, proc_kgid); \
	} while (0)

#define PCIE_PROCFS_ADD_FILE(_sdev, name, parent, mode)                      \
	do {                                                               \
		struct proc_dir_entry *entry = proc_create_data(# name, mode, parent, &pcie_procfs_ ## name ## _fops, _sdev); \
		PCIE_PROCFS_SET_UID_GID(entry);                            \
	} while (0)

#define PCIE_PROCFS_REMOVE_FILE(name, parent) remove_proc_entry(# name, parent)

int pcie_create_proc_dir(struct pcie_mif *pcie);
void pcie_remove_proc_dir(void);

#endif /* SCSC_PCIE_PROC_H */
