/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* temporary solution: Do not use these sysfs as official purpose */
/* these function are not official one. only purpose is for temporary test */

#if defined(CONFIG_DEBUG_FS) && !defined(CONFIG_SAMSUNG_PRODUCT_SHIP) && defined(CONFIG_SEC_GPIO_DVS) && defined(CONFIG_EXYNOS_DECON_MDNIE)
#include <linux/debugfs.h>
#include <linux/sort.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/vmalloc.h>

#include "mdnie.h"
#include "dd.h"

#define dbg_info(fmt, ...)	pr_info(pr_fmt("%s: %3d: %s: " fmt), "mdnie mdnie", __LINE__, __func__, ##__VA_ARGS__)
#define dbg_warn(fmt, ...)	pr_warn(pr_fmt("%s: %3d: %s: " fmt), "mdnie mdnie", __LINE__, __func__, ##__VA_ARGS__)

#define MDNIE_TUNING_PATH	"/data/mdnie/"

struct d_info {
	char path[PATH_MAX];
	unsigned int tuning;
	struct mdnie_table *latest;
	struct mdnie_table latest_save;

	struct mdnie_info *md;
};

static int mdnie_open_file(const char *path, char **fp)
{
	struct file *f;
	char *dp;
	long length;
	int ret = 0;
	loff_t pos = 0;
	mm_segment_t oldfs;

	if (!path) {
		dbg_warn("path is invalid. null\n");
		return -EPERM;
	}

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	f = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(f)) {
		dbg_warn("filp_open skip: %s\n", path);
		return -EINVAL;
	}

	if (!S_ISREG(file_inode(f)->i_mode)) {
		dbg_warn("file is invalid: %s\n", path);
		goto exit;
	}

	length = i_size_read(file_inode(f));
	if (length <= 0) {
		dbg_warn("file length is invalid %ld\n", length);
		ret = -EINVAL;
		goto exit;
	}

	dp = vzalloc(length);
	if (dp == NULL) {
		dbg_warn("failed to alloc size %ld\n", length);
		ret = -EPERM;
		goto exit;
	}

	ret = vfs_read(f, (void __user *)dp, length, &pos);
	if (ret != length) {
		dbg_warn("read size: %d, length: %ld\n", ret, length);
		vfree(dp);
		ret = -EPERM;
		goto exit;
	}

	*fp = dp;

exit:
	filp_close(f, 0);

	set_fs(oldfs);

	return ret;
}

static int mdnie_check_firmware(const char *path, char *name)
{
	char *fp, *p = NULL;
	int count = 0, size;

	size = mdnie_open_file(path, &fp);
	if (IS_ERR_OR_NULL(fp) || size <= 0) {
		dbg_warn("file open skip %s\n", path);
		vfree(p);
		return -EPERM;
	}

	p = fp;
	while ((p = strstr(p, name)) != NULL) {
		count++;
		p++;
	}

	vfree(fp);

	dbg_info("%s found %d times in %s\n", name, count, path);

	/* if count is 1, it means tuning header. if count is 0, it means tuning data */
	return count;
}

static uintptr_t mdnie_request_firmware(char *path, char *name, int *ret)
{
	char *token, *ptr = NULL;
	int numperline = 0, size, data[2];
	unsigned int count = 0, i;
	unsigned int *dp = NULL;

	size = mdnie_open_file(path, &ptr);
	if (IS_ERR_OR_NULL(ptr) || size <= 0) {
		dbg_warn("file open skip %s\n", path);
		*ret = -EPERM;
		goto exit;
	}

	dp = vzalloc(size * sizeof(*dp));
	if (dp == NULL) {
		dbg_warn("failed to alloc size %d\n", size);
		*ret = -ENOMEM;
		goto exit;
	}

	while (!IS_ERR_OR_NULL(ptr)) {
		ptr = (name) ? strstr(ptr, name) : ptr;
		while ((token = strsep(&ptr, "\n")) != NULL) {
			if (*token == '\0')
				continue;
			numperline = sscanf(token, "%8i, %8i", &data[0], &data[1]);
			if (numperline < 0)
				goto exit;
			dbg_info("sscanf: %2d, strlen: %2d, %s\n", numperline, (int)strlen(token), token);
			if (!numperline && strlen(token) <= 1) {
				dp[count] = 0xffff;
				dbg_info("stop at %d\n", count);
				if (count)
					count = (dp[count - 1] == dp[count]) ? count : count + 1;
				break;
			}
			for (i = 0; i < numperline; count++, i++)
				dp[count] = data[i];
		}
	}

	for (i = 0; i < count; i++)
		dbg_info("[%4d] %04x\n", i, dp[i]);

	*ret = count;

exit:
	vfree(ptr);
	/* return allocated address for prevent */
	return (uintptr_t)dp;
}

static uintptr_t mdnie_request_table(char *path, struct mdnie_table *org)
{
	unsigned int i, j = 0, k = 0, len;
	unsigned int *buf = NULL;
	mdnie_t *cmd = 0;
	int size, ret = 0, cmd_found = 0;

	ret = mdnie_check_firmware(path, org->name);
	if (ret < 0)
		goto exit;

	buf = (unsigned int *)mdnie_request_firmware(path, ret ? org->name : NULL, &size);
	if (size <= 0) {
		if (buf)
			vfree(buf);
		goto exit;
	}

	cmd = vzalloc(size * sizeof(mdnie_t));
	if (IS_ERR_OR_NULL(cmd))
		goto exit;

	for (i = 0; org->seq[i].len; i++) {
		if (!org->update_flag[i])
			continue;
		for (len = 0; k < size; len++, j++, k++) {
			if (buf[k] == 0xffff) {
				dbg_info("stop at %d, %d, %d\n", k, j, len);
				k++;
				break;
			}
			cmd[j] = buf[k];
			dbg_info("seq[%d].len[%3d], cmd[%3d]: %02x, buf[%3d]: %02x\n", i, len, j, cmd[j], k, buf[k]);
		}
		org->seq[i].cmd = &cmd[j - len];
		org->seq[i].len = len;
		cmd_found = 1;
		dbg_info("seq[%d].cmd: &cmd[%3d], seq[%d].len: %d\n", i, j - len, i, len);
	}

	vfree(buf);

	if (!cmd_found) {
		vfree(cmd);
		cmd = 0;
	}

	for (i = 0; org->seq[i].len; i++) {
		dbg_info("%d: size is %d\n", i, org->seq[i].len);
		for (j = 0; j < org->seq[i].len; j++)
			dbg_info("%d: %03d: %02x\n", i, j, org->seq[i].cmd[j]);
	}

exit:
	/* return allocated address for prevent */
	return (uintptr_t)cmd;
}

static int status_show(struct seq_file *m, void *unused)
{
	struct d_info *d = m->private;
	struct mdnie_info *mdnie = d->md;
	struct mdnie_table *table = NULL;
	int i, j;
	u8 *buffer;

	if (!mdnie->enable) {
		dbg_info("mdnie state is %s\n", mdnie->enable ? "on" : "off");
		seq_printf(m, "mdnie state is %s\n", mdnie->enable ? "on" : "off");
		goto exit;
	}

	table = d->latest;

	for (i = 0; table->seq[i].len; i++) {
		if (IS_ERR_OR_NULL(table->seq[i].cmd)) {
			dbg_info("mdnie sequence %s %dth command is null\n", table->name, i);
			goto exit;
		}
	}

	seq_printf(m, "+ %s\n", table->name);

	for (j = 0; table->seq[j].len; j++) {
		if (!table->update_flag[j]) {
			mdnie->ops.write(mdnie->data, &table->seq[j], 1);
			continue;
		}

		buffer = vzalloc(table->seq[j].len);

		mdnie->ops.read(mdnie->data, table->seq[j].cmd[0], buffer, table->seq[j].len - 1);

		seq_printf(m, "  0:\t0x%02x\t0x%02x\n", table->seq[j].cmd[0], table->seq[j].cmd[0]);
		for (i = 0; i < table->seq[j].len - 1; i++) {
			seq_printf(m, "%3d:\t0x%02x\t0x%02x", i + 1, table->seq[j].cmd[i+1], buffer[i]);
			if (table->seq[j].cmd[i+1] != buffer[i])
				seq_puts(m, "\t(X)");

			seq_puts(m, "\n");
		}

		vfree(buffer);
	}

	seq_printf(m, "- %s\n", table->name);

exit:
	return 0;
}

static int status_open(struct inode *inode, struct file *f)
{
	return single_open(f, status_show, inode->i_private);
}

static const struct file_operations status_fops = {
	.open		= status_open,
	.read		= seq_read,
	.llseek		= no_llseek,
	.release	= single_release,
};

static int tuning_show(struct seq_file *m, void *unused)
{
	struct d_info *d = m->private;
	struct mdnie_table *table = NULL;
	int i, idx;

	if (!d->tuning) {
		dbg_info("tuning mode is %s\n", d->tuning ? "on" : "off");
		seq_printf(m, "tuning mode is %s\n", d->tuning ? "on" : "off");
		goto exit;
	}

	seq_printf(m, "+%s\n", d->path);

	table = d->latest;
	if (!IS_ERR_OR_NULL(table) && !IS_ERR_OR_NULL(table->name)) {
		mdnie_request_table(d->path, table);
		for (idx = 0; table->seq[idx].len; idx++) {
			for (i = 0; i < table->seq[idx].len; i++)
				seq_printf(m, "0x%02x ", table->seq[idx].cmd[i]);
		}
		seq_puts(m, "\n");
	}

exit:
	seq_puts(m, "-\n");

	return 0;
}

static int tuning_open(struct inode *inode, struct file *f)
{
	return single_open(f, tuning_show, inode->i_private);
}

void mdnie_renew_table(struct mdnie_info *mdnie, struct mdnie_table *org)
{
	struct d_info *d = mdnie->dd_mdnie;

	mutex_lock(&mdnie->lock);

	if (unlikely(d->tuning && strlen(d->path))) {
		if (d->latest != org) {
			if (!IS_ERR_OR_NULL(d->latest))
				memcpy(d->latest, &d->latest_save, sizeof(struct mdnie_table));

			memcpy(&d->latest_save, org, sizeof(struct mdnie_table));
		}

		mdnie_request_table(d->path, org);
	}

	d->latest = org;

	mutex_unlock(&mdnie->lock);
}

static ssize_t tuning_store(struct file *f, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct seq_file *m = f->private_data;
	struct d_info *d = m->private;
	struct mdnie_info *mdnie = d->md;
	int ret = 0;
	const char *filename = NULL;
	char ibuf[NAME_MAX] = {0, };
	char *pbuf;
	unsigned int value;

	ret = dd_simple_write_to_buffer(ibuf, sizeof(ibuf), ppos, user_buf, count);
	if (ret < 0) {
		dbg_info("dd_simple_write_to_buffer fail: %d\n", ret);
		goto exit;
	}

	pbuf = ibuf;
	if (!strncmp(ibuf, "0", count - 1) || !strncmp(ibuf, "1", count - 1)) {
		ret = kstrtouint(ibuf, 0, &value);
		if (ret < 0)
			return count;

		dbg_info("%s -> %s\n", d->tuning ? "on" : "off", value ? "on" : "off");

		if (d->tuning == value)
			goto exit;

		/* on */
		if (!d->tuning && value) {
			mutex_lock(&mdnie->lock);
			if (!IS_ERR_OR_NULL(d->latest))
				memcpy(d->latest, &d->latest_save, sizeof(struct mdnie_table));

			mutex_unlock(&mdnie->lock);
		}

		/* off */
		if (d->tuning && !value) {
			mutex_lock(&mdnie->lock);
			memcpy(d->latest, &d->latest_save, sizeof(struct mdnie_table));
			memset(&d->latest_save, 0, sizeof(struct mdnie_table));
			memset(&d->path, 0, sizeof(d->path));
			mutex_unlock(&mdnie->lock);
		}

		d->tuning = value;
		if (d->tuning)
			goto exit;
	}

	filename = kbasename(pbuf);
	if (filename && !isalnum(*filename)) {
		dbg_warn("file name is invalid. %s\n", filename);
		return count;
	}

	memset(d->path, 0, sizeof(d->path));
	scnprintf(d->path, sizeof(d->path), "%s%s", MDNIE_TUNING_PATH, filename);

	mdnie_update(mdnie);

exit:
	return count;
}

static const struct file_operations tuning_fops = {
	.open		= tuning_open,
	.write		= tuning_store,
	.read		= seq_read,
	.llseek		= no_llseek,
	.release	= single_release,
};

static int help_show(struct seq_file *m, void *unused)
{
	seq_puts(m, "\n");
	seq_puts(m, "------------------------------------------------------------\n");
	seq_puts(m, "* ATTENTION\n");
	seq_puts(m, "* These sysfs can NOT be mandatory official purpose\n");
	seq_puts(m, "* These sysfs has risky, harmful, unstable function\n");
	seq_puts(m, "* So we can not support these sysfs for official use\n");
	seq_puts(m, "* DO NOT request improvement related with these function\n");
	seq_puts(m, "* DO NOT request these function as the mandatory requirements\n");
	seq_puts(m, "* If you insist, we eliminate these function immediately\n");
	seq_puts(m, "------------------------------------------------------------\n");
	seq_puts(m, "\n");
	seq_puts(m, "---------- usage\n");
	seq_puts(m, "# cd /d/dd_mdnie\n");
	seq_puts(m, "---------- status usage\n");
	seq_puts(m, "# cat status\n");
	seq_puts(m, "= see status value with latest read data from panel\n");
	seq_puts(m, "\n");
	seq_puts(m, "---------- tuning usage\n");
	seq_puts(m, "# mkdir /data/mdnie\n");
	seq_puts(m, "adb push auto_ui /data/mdnie\n");
	seq_puts(m, "# echo 1 > tuning\n");
	seq_puts(m, "# echo auto_ui > tuning\n");
	seq_puts(m, "# echo 0 > tuning\n");
	seq_puts(m, "\n");
	seq_puts(m, "1. first connet your device to pc and check adb working\n");
	seq_puts(m, "2. type below command in your target to create tune directory\n");
	seq_puts(m, "# mkdir /data/mdnie\n");
	seq_puts(m, "3. type below command in your pc to send tuning file\n");
	seq_puts(m, "adb push filename_of_tuning_file /data/mdnie\n");
	seq_puts(m, "ex) adb push auto_ui /data/mdie\n");
	seq_puts(m, "ex) adb push auto_camera /data/mdie\n");
	seq_puts(m, "4. type below command in your target to start tuning mode\n");
	seq_puts(m, "# echo 1 > tuning\n");
	seq_puts(m, "5. type below command in your target to assign tuning file\n");
	seq_puts(m, "# echo filename_of_tuning_file > tuning\n");
	seq_puts(m, "ex) # echo auto_ui > tuning\n");
	seq_puts(m, "ex) # echo auto_camera > tuning\n");
	seq_puts(m, "ex) # echo blahblahblah > tuning\n");
	seq_puts(m, "6. now mdnie driver works with assigned tuning file\n");
	seq_puts(m, "7. type below command to end tuning mode\n");
	seq_puts(m, "# echo 0 > tuning\n\n");

	return 0;
}

static int help_open(struct inode *inode, struct file *file)
{
	return single_open(file, help_show, inode->i_private);
}

static const struct file_operations help_fops = {
	.open		= help_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

int init_debugfs_mdnie(struct mdnie_info *md, unsigned int mdnie_no)
{
	char name[NAME_MAX] = {0, };
	int ret = 0;
	static struct dentry *debugfs_root;
	struct d_info *d;

	if (!md) {
		dbg_warn("failed to get mdnie_device\n");
		ret = -ENODEV;
		goto exit;
	}

	dbg_info("+\n");

	d = kzalloc(sizeof(struct d_info), GFP_KERNEL);

	if (!debugfs_root) {
		debugfs_root = debugfs_create_dir("dd_mdnie", NULL);
		debugfs_create_file("_help", 0400, debugfs_root, md, &help_fops);
	}

	d->md = md;
	md->dd_mdnie = d;

	memset(name, 0, sizeof(name));
	scnprintf(name, sizeof(name), !mdnie_no ? "tuning" : "tuning.%d", mdnie_no);
	debugfs_create_file(name, 0600, debugfs_root, d, &tuning_fops);

	memset(name, 0, sizeof(name));
	scnprintf(name, sizeof(name), !mdnie_no ? "status" : "status.%d", mdnie_no);
	debugfs_create_file(name, 0400, debugfs_root, d, &status_fops);

	dbg_info("-\n");

exit:
	return ret;
}
#endif

