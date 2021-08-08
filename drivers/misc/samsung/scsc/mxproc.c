/****************************************************************************
 *
 * Copyright (c) 2014 - 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/seq_file.h>
#include <scsc/scsc_release.h>
#include <scsc/scsc_logring.h>
#include "mxman.h"
#include "mxproc.h"
#ifdef CONFIG_SCSC_WLBTD
#include "scsc_wlbtd.h"
#endif

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#include <linux/uaccess.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <soc/samsung/debug-snapshot.h>
#else
#include <linux/debug-snapshot.h>
#endif
#endif

#ifndef AID_MXPROC
#define AID_MXPROC 0
#endif

#define MX_PROCFS_RW_FILE_OPS(name)                                           \
	static ssize_t mx_procfs_ ## name ## _write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos); \
	static ssize_t                      mx_procfs_ ## name ## _read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos); \
	static const struct file_operations mx_procfs_ ## name ## _fops = { \
		.read = mx_procfs_ ## name ## _read,                        \
		.write = mx_procfs_ ## name ## _write,                      \
		.open = mx_procfs_generic_open,                     \
		.llseek = generic_file_llseek                                 \
	}
#define MX_PROCFS_RO_FILE_OPS(name)                                           \
	static ssize_t                      mx_procfs_ ## name ## _read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos); \
	static const struct file_operations mx_procfs_ ## name ## _fops = { \
		.read = mx_procfs_ ## name ## _read,                        \
		.open = mx_procfs_generic_open,                                  \
		.llseek = generic_file_llseek                               \
	}


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
#define MX_PDE_DATA(inode) PDE_DATA(inode)
#else
#define MX_PDE_DATA(inode) (PDE(inode)->data)
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
#define MX_PROCFS_SET_UID_GID(_entry) \
	do { \
		kuid_t proc_kuid = KUIDT_INIT(AID_MXPROC); \
		kgid_t proc_kgid = KGIDT_INIT(AID_MXPROC); \
		proc_set_user(_entry, proc_kuid, proc_kgid); \
	} while (0)
#else
#define MX_PROCFS_SET_UID_GID(entry) \
	do { \
		(entry)->uid = AID_MXPROC; \
		(entry)->gid = AID_MXPROC; \
	} while (0)
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 9))
#define MX_PROCFS_ADD_FILE(_sdev, name, parent, mode)                      \
	do {                                                               \
		struct proc_dir_entry *entry = proc_create_data(# name, mode, parent, &mx_procfs_ ## name ## _fops, _sdev); \
		MX_PROCFS_SET_UID_GID(entry);                              \
	} while (0)
#else
#define MX_PROCFS_ADD_FILE(_data, name, parent, mode)                      \
	do {                                                               \
		struct proc_dir_entry *entry;                              \
		entry = create_proc_entry(# name, mode, parent);           \
		if (entry) {                                               \
			entry->proc_fops = &mx_procfs_ ## name ## _fops; \
			entry->data = _data;                               \
			MX_PROCFS_SET_UID_GID(entry);                      \
		}                                                          \
	} while (0)
#endif

#define MX_PROCFS_REMOVE_FILE(name, parent) remove_proc_entry(# name, parent)

#define OS_UNUSED_PARAMETER(x) ((void)(x))
#define MX_DIRLEN 128
static const char *procdir_ctrl = "driver/mxman_ctrl";
static const char *procdir_info = "driver/mxman_info";

static int mx_procfs_generic_open(struct inode *inode, struct file *file)
{
	file->private_data = MX_PDE_DATA(inode);
	return 0;
}

static ssize_t mx_procfs_mx_fail_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	struct mxproc *mxproc = file->private_data;

	OS_UNUSED_PARAMETER(mxproc);
	OS_UNUSED_PARAMETER(file);
	OS_UNUSED_PARAMETER(user_buf);
	OS_UNUSED_PARAMETER(count);
	OS_UNUSED_PARAMETER(ppos);

	SCSC_TAG_DEBUG(MX_PROC, "OK\n");
	return 0;
}

static ssize_t mx_procfs_mx_fail_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct mxproc *mxproc = file->private_data;

	OS_UNUSED_PARAMETER(file);
	OS_UNUSED_PARAMETER(user_buf);
	OS_UNUSED_PARAMETER(count);
	OS_UNUSED_PARAMETER(ppos);

	if (mxproc)
		mxman_fail(mxproc->mxman, SCSC_PANIC_CODE_HOST << 15, __func__);
	SCSC_TAG_DEBUG(MX_PROC, "OK\n");

	return count;
}

static ssize_t mx_procfs_mx_freeze_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	struct mxproc *mxproc = file->private_data;

	OS_UNUSED_PARAMETER(mxproc);
	OS_UNUSED_PARAMETER(file);
	OS_UNUSED_PARAMETER(user_buf);
	OS_UNUSED_PARAMETER(count);
	OS_UNUSED_PARAMETER(ppos);

	SCSC_TAG_DEBUG(MX_PROC, "OK\n");
	return 0;
}

static ssize_t mx_procfs_mx_freeze_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct mxproc *mxproc = file->private_data;

	OS_UNUSED_PARAMETER(file);
	OS_UNUSED_PARAMETER(user_buf);
	OS_UNUSED_PARAMETER(count);
	OS_UNUSED_PARAMETER(ppos);

	if (mxproc)
		mxman_freeze(mxproc->mxman);
	SCSC_TAG_INFO(MX_PROC, "OK\n");

	return count;
}

static ssize_t mx_procfs_mx_panic_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	struct mxproc *mxproc = file->private_data;

	OS_UNUSED_PARAMETER(mxproc);
	OS_UNUSED_PARAMETER(file);
	OS_UNUSED_PARAMETER(user_buf);
	OS_UNUSED_PARAMETER(count);
	OS_UNUSED_PARAMETER(ppos);

	/* Force a FW panic on read as well as write to allow test in user builds */
	if (mxproc)
		mxman_force_panic(mxproc->mxman);
	SCSC_TAG_INFO(MX_PROC, "OK\n");

	return 0;
}

static ssize_t mx_procfs_mx_panic_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	char value = 0;
	struct mxproc *mxproc = file->private_data;

	OS_UNUSED_PARAMETER(value);
	OS_UNUSED_PARAMETER(file);
	OS_UNUSED_PARAMETER(user_buf);
	OS_UNUSED_PARAMETER(count);
	OS_UNUSED_PARAMETER(ppos);

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT) && defined(GO_S2D_ID)
	if (count != 2)
		return -EFAULT;
	if (copy_from_user(&value, user_buf, 1))
		return -EFAULT;
	if (value == '3') {
		SCSC_TAG_INFO(MX_PROC, "Manual Scandump");
		dbg_snapshot_do_dpm_policy(GO_S2D_ID);
	} else if (mxproc) {
		SCSC_TAG_INFO(MX_PROC, "Manual FW Panic");
		mxman_force_panic(mxproc->mxman);
	}
#else
	if (mxproc)
		mxman_force_panic(mxproc->mxman);
#endif
	SCSC_TAG_INFO(MX_PROC, "OK\n");

	return count;
}

static ssize_t mx_procfs_mx_lastpanic_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	struct mxproc *mxproc = file->private_data;
	char buf[64];
	int bytes;

	if (!mxproc || !mxproc->mxman)
		return -EINVAL;

	memset(buf, '\0', sizeof(buf));

	bytes = snprintf(buf, sizeof(buf), "scsc_panic_code    : 0x%x\nscsc_panic_subcode : 0x%x\n",
			(mxproc->mxman->scsc_panic_code), (mxproc->mxman->scsc_panic_code & 0x7FFF));

	return simple_read_from_buffer(user_buf, count, ppos, buf, bytes);
}

static ssize_t mx_procfs_mx_suspend_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	struct mxproc *mxproc = file->private_data;
	char buf[3];

	OS_UNUSED_PARAMETER(file);

	buf[0] = mxproc->mxman->suspended ? 'Y' : 'N';
	buf[1] = '\n';
	buf[2] = '\0';

	SCSC_TAG_INFO(MX_PROC, "suspended: %c\n", buf[0]);

	return simple_read_from_buffer(user_buf, count, ppos, buf, sizeof(buf));
}

static ssize_t mx_procfs_mx_suspend_count_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	struct mxproc *mxproc = file->private_data;
	int pos = 0;
	char buf[32];
	const size_t bufsz = sizeof(buf);
	u32 suspend_count;

	OS_UNUSED_PARAMETER(file);

	if (!mxproc || !mxproc->mxman)
		return 0;

	suspend_count = atomic_read(&mxproc->mxman->suspend_count);
	SCSC_TAG_INFO(MX_PROC, "suspend_count: %u\n", suspend_count);
	pos += scnprintf(buf + pos, bufsz - pos, "%u\n", suspend_count);

	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}

static ssize_t mx_procfs_mx_recovery_count_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	struct mxproc *mxproc = file->private_data;
	int pos = 0;
	char buf[32];
	const size_t bufsz = sizeof(buf);
	u32 recovery_count;

	OS_UNUSED_PARAMETER(file);

	if (!mxproc || !mxproc->mxman)
		return 0;

	recovery_count = atomic_read(&mxproc->mxman->recovery_count);
	SCSC_TAG_INFO(MX_PROC, "recovery_count: %u\n", recovery_count);
	pos += scnprintf(buf + pos, bufsz - pos, "%u\n", recovery_count);

	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}

static ssize_t mx_procfs_mx_boot_count_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	struct mxproc *mxproc = file->private_data;
	int pos = 0;
	char buf[32];
	const size_t bufsz = sizeof(buf);
	u32 boot_count;

	OS_UNUSED_PARAMETER(file);

	if (!mxproc || !mxproc->mxman)
		return 0;

	boot_count = atomic_read(&mxproc->mxman->boot_count);
	SCSC_TAG_INFO(MX_PROC, "boot_count: %u\n", boot_count);
	pos += scnprintf(buf + pos, bufsz - pos, "%u\n", boot_count);

	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}

static ssize_t mx_procfs_mx_suspend_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct mxproc *mxproc = file->private_data;
	int r;

	OS_UNUSED_PARAMETER(file);
	OS_UNUSED_PARAMETER(ppos);

	if (count && mxproc) {
		switch (user_buf[0]) {
		case 'Y':
			SCSC_TAG_INFO(MX_PROC, "force suspend\n");
			r = mxman_suspend(mxproc->mxman);
			if (r) {
				SCSC_TAG_INFO(MX_PROC, "mx_suspend failed %d\n", r);
				return r;
			}
			break;
		case 'N':
			SCSC_TAG_INFO(MX_PROC, "force resume\n");
			mxman_resume(mxproc->mxman);
			break;
		default:
			SCSC_TAG_INFO(MX_PROC, "invalid value %c\n", user_buf[0]);
			return -EINVAL;
		}
	}

	return count;
}

static ssize_t mx_procfs_mx_status_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	struct mxproc *mxproc = file->private_data;
	char         buf[32];
	int          pos = 0;
	const size_t bufsz = sizeof(buf);

	if (!mxproc || !mxproc->mxman)
		return 0;

	switch (mxproc->mxman->mxman_state) {
	case MXMAN_STATE_STOPPED:
		pos += scnprintf(buf + pos, bufsz - pos, "%s\n", "MXMAN_STATE_STOPPED");
		break;
	case MXMAN_STATE_STARTING:
		pos += scnprintf(buf + pos, bufsz - pos, "%s\n", "MXMAN_STATE_STARTING");
		break;
	case MXMAN_STATE_STARTED:
		pos += scnprintf(buf + pos, bufsz - pos, "%s\n", "MXMAN_STATE_STARTED");
		break;
	case MXMAN_STATE_FAILED:
		pos += scnprintf(buf + pos, bufsz - pos, "%s\n", "MXMAN_STATE_FAILED");
		break;
	case MXMAN_STATE_FROZEN:
		pos += scnprintf(buf + pos, bufsz - pos, "%s\n", "MXMAN_STATE_FROZEN");
		break;
	default:
		return 0;
	}

	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}

static ssize_t mx_procfs_mx_services_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	struct mxproc *mxproc = file->private_data;
	char         buf[256];
	int          pos = 0;
	const size_t bufsz = sizeof(buf);

	if (!mxproc || !mxproc->mxman)
		return 0;

	pos = scsc_mx_list_services(mxproc->mxman, buf, bufsz);

	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}

static ssize_t mx_procfs_mx_wlbt_stat_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	struct mxproc *mxproc = file->private_data;
	struct scsc_mif_abs *mif_abs;
	int pos = 0;
	int r;
	char buf[32];
	const size_t bufsz = sizeof(buf);
	u32 val = 0xff;

	OS_UNUSED_PARAMETER(file);

	if (!mxproc || !mxproc->mxman || !mxproc->mxman->mx)
		return 0;

	mif_abs = scsc_mx_get_mif_abs(mxproc->mxman->mx);

	/* Read WLBT_STAT register */
	if (mif_abs->mif_read_register) {
		r = mif_abs->mif_read_register(mif_abs, SCSC_REG_READ_WLBT_STAT, &val);
		if (r)
			val = 0xff; /* failed */
	}

	pos += scnprintf(buf + pos, bufsz - pos, "0x%x\n", val);

	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}

MX_PROCFS_RW_FILE_OPS(mx_fail);
MX_PROCFS_RW_FILE_OPS(mx_freeze);
MX_PROCFS_RW_FILE_OPS(mx_panic);
MX_PROCFS_RW_FILE_OPS(mx_suspend);
MX_PROCFS_RO_FILE_OPS(mx_suspend_count);
MX_PROCFS_RO_FILE_OPS(mx_recovery_count);
MX_PROCFS_RO_FILE_OPS(mx_boot_count);
MX_PROCFS_RO_FILE_OPS(mx_status);
MX_PROCFS_RO_FILE_OPS(mx_services);
MX_PROCFS_RO_FILE_OPS(mx_lastpanic);
MX_PROCFS_RO_FILE_OPS(mx_wlbt_stat);

static u32 proc_count;

int mxproc_create_ctrl_proc_dir(struct mxproc *mxproc, struct mxman *mxman)
{
	char                  dir[MX_DIRLEN];
	struct proc_dir_entry *parent;

	(void)snprintf(dir, sizeof(dir), "%s%d", procdir_ctrl, proc_count);
	parent = proc_mkdir(dir, NULL);
	if (!parent) {
		SCSC_TAG_ERR(MX_PROC, "failed to create proc dir %s\n", procdir_ctrl);
		return -EINVAL;
	}
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 4, 0))
	parent->data = mxproc;
#endif
	mxproc->mxman = mxman;
	mxproc->procfs_ctrl_dir = parent;
	mxproc->procfs_ctrl_dir_num = proc_count;
	MX_PROCFS_ADD_FILE(mxproc, mx_fail, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	MX_PROCFS_ADD_FILE(mxproc, mx_freeze, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	MX_PROCFS_ADD_FILE(mxproc, mx_panic, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	MX_PROCFS_ADD_FILE(mxproc, mx_suspend, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	MX_PROCFS_ADD_FILE(mxproc, mx_suspend_count, parent, S_IRUSR | S_IRGRP | S_IROTH);
	MX_PROCFS_ADD_FILE(mxproc, mx_recovery_count, parent, S_IRUSR | S_IRGRP | S_IROTH);
	MX_PROCFS_ADD_FILE(mxproc, mx_status, parent, S_IRUSR | S_IRGRP | S_IROTH);
	MX_PROCFS_ADD_FILE(mxproc, mx_services, parent, S_IRUSR | S_IRGRP | S_IROTH);
	MX_PROCFS_ADD_FILE(mxproc, mx_lastpanic, parent, S_IRUSR | S_IRGRP | S_IROTH);
	MX_PROCFS_ADD_FILE(mxproc, mx_wlbt_stat, parent, S_IRUSR | S_IRGRP | S_IROTH);

	SCSC_TAG_DEBUG(MX_PROC, "created %s proc dir\n", dir);
	proc_count++;

	return 0;
}

void mxproc_remove_ctrl_proc_dir(struct mxproc *mxproc)
{
	if (mxproc->procfs_ctrl_dir) {
		char dir[MX_DIRLEN];

		MX_PROCFS_REMOVE_FILE(mx_fail, mxproc->procfs_ctrl_dir);
		MX_PROCFS_REMOVE_FILE(mx_freeze, mxproc->procfs_ctrl_dir);
		MX_PROCFS_REMOVE_FILE(mx_panic, mxproc->procfs_ctrl_dir);
		MX_PROCFS_REMOVE_FILE(mx_suspend, mxproc->procfs_ctrl_dir);
		MX_PROCFS_REMOVE_FILE(mx_suspend_count, mxproc->procfs_ctrl_dir);
		MX_PROCFS_REMOVE_FILE(mx_recovery_count, mxproc->procfs_ctrl_dir);
		MX_PROCFS_REMOVE_FILE(mx_status, mxproc->procfs_ctrl_dir);
		MX_PROCFS_REMOVE_FILE(mx_services, mxproc->procfs_ctrl_dir);
		MX_PROCFS_REMOVE_FILE(mx_lastpanic, mxproc->procfs_ctrl_dir);
		MX_PROCFS_REMOVE_FILE(mx_wlbt_stat, mxproc->procfs_ctrl_dir);
		(void)snprintf(dir, sizeof(dir), "%s%d", procdir_ctrl, mxproc->procfs_ctrl_dir_num);
		remove_proc_entry(dir, NULL);
		mxproc->procfs_ctrl_dir = NULL;
		proc_count--;
		SCSC_TAG_DEBUG(MX_PROC, "removed %s proc dir\n", dir);
	}
}

static ssize_t mx_procfs_mx_rf_hw_ver_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[256];
	int bytes;
	struct mxproc *mxproc = file->private_data;

	if (!mxproc || !mxproc->mxman)
		return -EINVAL;

	memset(buf, '\0', sizeof(buf));
	bytes = snprintf(buf, sizeof(buf), "RF version: 0x%04x\n", (mxproc->mxman->rf_hw_ver));

	return simple_read_from_buffer(user_buf, count, ppos, buf, bytes);
}

MX_PROCFS_RO_FILE_OPS(mx_rf_hw_ver);

static ssize_t mx_procfs_mx_rf_hw_name_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[256];
	int bytes;
	struct mxproc *mxproc = file->private_data;

	if (!mxproc || !mxproc->mxman)
		return -EINVAL;

	memset(buf, '\0', sizeof(buf));

	bytes = mxman_print_rf_hw_version(mxproc->mxman, buf, sizeof(buf));

	return simple_read_from_buffer(user_buf, count, ppos, buf, bytes);
}

MX_PROCFS_RO_FILE_OPS(mx_rf_hw_name);

static ssize_t mx_procfs_mx_release_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[256];
	int bytes;
	struct mxproc *mxproc = file->private_data;
	char *build_id = 0;

	OS_UNUSED_PARAMETER(file);

	if (mxproc && mxproc->mxman)
		build_id = mxproc->mxman->fw_build_id;

	memset(buf, '\0', sizeof(buf));

	bytes = snprintf(buf, sizeof(buf), "Release: %d.%d.%d.%d.%d (f/w: %s)\n",
		SCSC_RELEASE_PRODUCT, SCSC_RELEASE_ITERATION, SCSC_RELEASE_CANDIDATE, SCSC_RELEASE_POINT, SCSC_RELEASE_CUSTOMER,
		build_id ? build_id : "unknown");

	if (bytes > sizeof(buf))
		bytes = sizeof(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, bytes);
}

MX_PROCFS_RO_FILE_OPS(mx_release);

static ssize_t mx_procfs_mx_ttid_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[256];
	int bytes;
	struct mxproc *mxproc = file->private_data;
	char *id = 0;

	OS_UNUSED_PARAMETER(file);

	if (mxproc && mxproc->mxman)
		id = mxproc->mxman->fw_ttid;

	memset(buf, '\0', sizeof(buf));

	if (id)
		bytes = snprintf(buf, sizeof(buf), "%s\n", id);
	else
		bytes = snprintf(buf, sizeof(buf), "%s\n", "FW_TTID not defined");

	if (bytes > sizeof(buf))
		bytes = sizeof(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, bytes);
}

MX_PROCFS_RO_FILE_OPS(mx_ttid);

int mxproc_create_info_proc_dir(struct mxproc *mxproc, struct mxman *mxman)
{
	char                  dir[MX_DIRLEN];
	struct proc_dir_entry *parent;

	(void)snprintf(dir, sizeof(dir), "%s", procdir_info);
	parent = proc_mkdir(dir, NULL);
	if (!parent) {
		SCSC_TAG_ERR(MX_PROC, "failed to create /proc dir\n");
		return -EINVAL;
	}
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 4, 0))
	parent->data = mxproc;
#endif
	mxproc->mxman = mxman;
	mxproc->procfs_info_dir = parent;
	MX_PROCFS_ADD_FILE(mxproc, mx_release, parent, S_IRUSR | S_IRGRP | S_IROTH);
	MX_PROCFS_ADD_FILE(mxproc, mx_rf_hw_ver, parent, S_IRUSR | S_IRGRP | S_IROTH);
	MX_PROCFS_ADD_FILE(mxproc, mx_rf_hw_name, parent, S_IRUSR | S_IRGRP | S_IROTH);
	MX_PROCFS_ADD_FILE(mxproc, mx_boot_count, parent, S_IRUSR | S_IRGRP | S_IROTH);
	MX_PROCFS_ADD_FILE(mxproc, mx_ttid, parent, S_IRUSR | S_IRGRP | S_IROTH);
	SCSC_TAG_DEBUG(MX_PROC, "created %s proc dir\n", dir);

	return 0;
}

void mxproc_remove_info_proc_dir(struct mxproc *mxproc)
{
	if (mxproc->procfs_info_dir) {
		char dir[MX_DIRLEN];

		MX_PROCFS_REMOVE_FILE(mx_ttid, mxproc->procfs_ctrl_dir);
		MX_PROCFS_REMOVE_FILE(mx_boot_count, mxproc->procfs_ctrl_dir);
		MX_PROCFS_REMOVE_FILE(mx_release, mxproc->procfs_info_dir);
		MX_PROCFS_REMOVE_FILE(mx_rf_hw_ver, mxproc->procfs_info_dir);
		MX_PROCFS_REMOVE_FILE(mx_rf_hw_name, mxproc->procfs_info_dir);
		(void)snprintf(dir, sizeof(dir), "%s", procdir_info);
		remove_proc_entry(dir, NULL);
		mxproc->procfs_info_dir = NULL;
		SCSC_TAG_DEBUG(MX_PROC, "removed %s proc dir\n", dir);
	}
}
