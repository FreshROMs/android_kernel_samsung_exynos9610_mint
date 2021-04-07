/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* temporary solution: Do not use these sysfs as official purpose */
/* these function are not official one. only purpose is for temporary test */

#if defined(CONFIG_DEBUG_FS) && !defined(CONFIG_SAMSUNG_PRODUCT_SHIP) && defined(CONFIG_SEC_GPIO_DVS)
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/module.h>

#include "../dsim.h"
#include "../decon.h"

#include "dd.h"

#if defined(CONFIG_DISPLAY_DEBUG)	/* this is not defconfig */
#define dbg_dbg(fmt, ...)	pr_debug(pr_fmt("%s: %3d: %s: " fmt), "lcd panel", __LINE__, __func__, ##__VA_ARGS__)
#else
#define dbg_dbg(fmt, ...)
#endif
#define dbg_info(fmt, ...)	pr_info(pr_fmt("%s: %3d: %s: " fmt), "lcd panel", __LINE__, __func__, ##__VA_ARGS__)
#define dbg_warn(fmt, ...)	pr_warn(pr_fmt("%s: %3d: %s: " fmt), "lcd panel", __LINE__, __func__, ##__VA_ARGS__)

#define STREQ(a, b)			(*(a) == *(b) && strcmp((a), (b)) == 0)
#define STRNEQ(a, b)			(strncmp((a), (b), (strlen(a))) == 0)

struct params_list_info {
	char			*name;
	struct list_head	node;

	unsigned int	max_size;
	unsigned int	max_type;

	unsigned int	max_h;
};

static struct params_list_info	*params_lists[10];

struct param_info {
	union {
		u8 *ptr_u08;
		u32 *ptr_u32;
	};

	u32 ptr_type;
	u32 ptr_size;

	union {
		u8 *org_u08;
		u32 *org_u32;
	};

	struct list_head	node;
};

static ssize_t param_write(struct file *f, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct params_list_info *params_list = ((struct seq_file *)f->private_data)->private;
	struct param_info *param = NULL;

	unsigned char ibuf[MAX_INPUT] = {0, };
	unsigned int tbuf[MAX_INPUT] = {0, };
	unsigned int value = 0, end = 0, input_w = 0, input_h = 0, offset_w = 0, offset_h = 0, param_old, param_new;
	char *pbuf, *token = NULL;
	int ret = 0;

	ret = dd_simple_write_to_buffer(ibuf, sizeof(ibuf), ppos, user_buf, count);
	if (ret < 0) {
		dbg_info("dd_simple_write_to_buffer fail: %d\n", ret);
		goto exit;
	}

	if (!strncmp(ibuf, "0", count - 1)) {
		dbg_info("input is 0(zero). reset param to default\n");

		list_for_each_entry(param, &params_list->node, node) {
			if (param->ptr_type == U8_MAX)
				memcpy(param->ptr_u08, param->org_u08, param->ptr_size * sizeof(u8));
			else if (param->ptr_type == U32_MAX)
				memcpy(param->ptr_u32, param->org_u32, param->ptr_size * sizeof(u32));
		}

		goto exit;
	}

	pbuf = ibuf;
	/* scan x, y first */
	while ((token = strsep(&pbuf, ", "))) {
		if (*token == '\0')
			continue;
		ret = kstrtou32(token, 0, &value);
		if (ret < 0 || end == ARRAY_SIZE(tbuf))
			break;

		dbg_info("[%2d] 0x%02x(%4d), %s\n", end, value, value, token);
		tbuf[end] = value;
		end++;
		if (end >= 2)
			break;
	}

	dbg_info("end: %d\n", end);

	if (ret < 0) {
		dbg_info("invalid input: ret(%d), %s\n", ret, user_buf);
		goto exit;
	}

	input_w = tbuf[0];
	input_h = tbuf[1];

	if (unlikely(list_empty(&params_list->node))) {
		dbg_info("list_empty of %s\n", params_list->name ? params_list->name : "null");
		goto exit;
	}

	list_for_each_entry(param, &params_list->node, node) {
		if (offset_h == input_h) {
			dbg_info("%dth param type(%d), size(%d)\n", offset_h, param->ptr_type, param->ptr_size);
			break;
		}
		offset_h++;
	}

	if (offset_h >= params_list->max_h) {
		dbg_info("invalid position: h(%d), max_h(%d)\n", offset_h, params_list->max_h);
		goto exit;
	}

	if (!param) {
		dbg_info("invalid param\n");
		goto exit;
	}

	/* scan remain */
	while ((token = strsep(&pbuf, ", "))) {
		if (*token == '\0')
			continue;
		if (param->ptr_type == U8_MAX)
			ret = kstrtou32(token, 16, &value);
		else if (param->ptr_type == U32_MAX)
			ret = kstrtou32(token, 0, &value);
		if (ret < 0 || end == ARRAY_SIZE(tbuf))
			break;

		dbg_info("[%2d] 0x%02x(%4d), %s\n", end, value, value, token);
		tbuf[end] = value;
		end++;
	}

	if (ret < 0) {
		dbg_info("invalid input: ret(%d), %s\n", ret, user_buf);
		goto exit;
	}

	if (end < 3 || end == ARRAY_SIZE(tbuf)) {
		dbg_info("invalid input: end(%d), input should be 3~%zu\n", end, ARRAY_SIZE(tbuf) - 1);
		goto exit;
	}

	end -= 2;

	dbg_info("end: %d\n", end);

	dbg_info("input_w(%d), input_h(%d), end(%d), max_w(%d), max_h(%d)\n", input_w, input_h, end, param->ptr_size, params_list->max_h);

	if (input_w + end - 1 > param->ptr_size) {
		dbg_info("invalid position: w(%d) + end(%d) - 1 <= max_w(%d)\n", input_w, end, param->ptr_size);
		goto exit;
	}

	if (param->ptr_type == U8_MAX) {
		for (offset_w = 0; offset_w < end; offset_w++) {
			param_old = param->ptr_u08[input_w + offset_w];
			param_new = tbuf[2 + offset_w];
			param_new = (param_new > U8_MAX) ? U8_MAX : param_new;

			dbg_info("[%2d] 0x%02x -> 0x%02x%s\n", input_w + offset_w, param_old, param_new, (param_old != param_new) ? " (!)" : "");
			param->ptr_u08[input_w + offset_w] = param_new;
		}
	} else if (param->ptr_type == U32_MAX) {
		for (offset_w = 0; offset_w < end; offset_w++) {
			param_old =  param->ptr_u32[input_w + offset_w];
			param_new = tbuf[2 + offset_w];
			param_new = (param_new > U32_MAX) ? U32_MAX : param_new;

			dbg_info("[%2d] %d -> %d%s\n", input_w + offset_w, param_old, param_new, (param_old != param_new) ? " (!)" : "");
			param->ptr_u32[input_w + offset_w] = param_new;
		}
	}

exit:
	return count;
}

static int param_show(struct seq_file *m, void *unused)
{
	struct params_list_info *params_list = m->private;
	struct param_info *param = NULL;
	u32 i = 0, j = 0, changed = 0;

	seq_puts(m, "  |");
	for (i = 0; i < params_list->max_size; i++)
		seq_printf(m, (params_list->max_type == 32) ? " %4d" : " %2d", i);
	seq_puts(m, "| <- input X first\n");
	seq_puts(m, "--+");
	for (i = 0; i < params_list->max_size * ((params_list->max_type == U32_MAX) ? 5 : 3) ; i++)
		seq_puts(m, "-");
	seq_puts(m, "\n");

	i = 0;
	if (params_list->max_type == U8_MAX) {
		list_for_each_entry(param, &params_list->node, node) {
			changed = memcmp(param->org_u08, param->ptr_u08, param->ptr_size);
			seq_printf(m, "%2d| %*ph%s\n", i, param->ptr_size, param->ptr_u08, changed ? " (!)" : "");
			i++;
		}
	} else if (params_list->max_type == U32_MAX) {
		list_for_each_entry(param, &params_list->node, node) {
			changed = memcmp(param->org_u32, param->ptr_u32, param->ptr_size);

			seq_printf(m, "%2d|", i);
			for (j = 0; j < param->ptr_size; j++)
				seq_printf(m, " %4d", param->ptr_u32[j]);
			seq_printf(m, "%s\n", changed ? " (!)" : "");
			i++;
		}
	}

	seq_puts(m, "\n");
	seq_printf(m, "# echo 1 0 0x34 > %s\n", params_list->name);
	seq_printf(m, "# echo 1 0 3456 > %s\n", params_list->name);
	seq_printf(m, "# echo X(dec) Y(dec) val_1 ... val_N > %s (N <= %d if X is 0)\n", params_list->name, params_list->max_size);

	return 0;
}

static int param_open(struct inode *inode, struct file *f)
{
	return single_open(f, param_show, inode->i_private);
}

static const struct file_operations param_fops = {
	.open		= param_open,
	.write		= param_write,
	.read		= seq_read,
	.llseek		= no_llseek,
	.release	= single_release,
};

static int help_show(struct seq_file *m, void *unused)
{
	int i = 0;
	struct params_list_info *params_list = NULL;

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
	seq_puts(m, "# cd /d/dd_param\n");
	seq_puts(m, "----------\n");

	for (i = 0; i < ARRAY_SIZE(params_lists); i++) {
		if (IS_ERR_OR_NULL(params_lists[i]))
			continue;

		params_list = params_lists[i];

		if (unlikely(list_empty(&params_list->node))) {
			seq_printf(m, "%s is empty\n", params_list->name);
			continue;
		}

		seq_printf(m, "# echo 1 0 0x34 > %s\n", params_list->name);
		seq_printf(m, "# echo 1 0 3456 > %s\n", params_list->name);
		seq_printf(m, "# echo X(dec) Y(dec) val_1 ... val_N > %s (N <= %d if X is 0)\n", params_list->name, params_list->max_size);
	}
	seq_puts(m, "----------\n");

	for (i = 0; i < ARRAY_SIZE(params_lists); i++) {
		if (IS_ERR_OR_NULL(params_lists[i]))
			continue;

		params_list = params_lists[i];

		if (unlikely(list_empty(&params_list->node))) {
			seq_printf(m, "%s is empty\n", params_list->name);
			continue;
		}

		seq_printf(m, "# echo 0 > %s\n", params_list->name);
	}
	seq_puts(m, "'echo 0' is for reset param to default\n");
	seq_puts(m, "----------\n");

	seq_puts(m, "you can write sequential parameter starts from X\n");
	seq_puts(m, "you can NOT write X(width,horizontal) exceed each row's max X\n");
	seq_puts(m, "you can NOT write sequential parameter exceed each row's max X\n");

	return 0;
}

static int help_open(struct inode *inode, struct file *f)
{
	return single_open(f, help_show, inode->i_private);
}

static const struct file_operations help_fops = {
	.open		= help_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int add_param(struct params_list_info *params_list, void *ptr, u32 ptr_type, u32 ptr_size)
{
	struct param_info *param;

	if (params_list->max_h > U8_MAX) {
		dbg_info("params_list->max_h(%d) invalid\n", params_list->max_h);
		return 0;
	}

	param = kzalloc(sizeof(struct param_info), GFP_KERNEL);

	if (ptr_type == U8_MAX) {
		param->ptr_u08 = (u8 *)ptr;
		param->org_u08 = kmemdup(ptr, ptr_size * sizeof(u8), GFP_KERNEL);
	} else if (ptr_type == U32_MAX) {
		param->ptr_u32 = (u32 *)ptr;
		param->org_u32 = kmemdup(ptr, ptr_size * sizeof(u32), GFP_KERNEL);
	}

	param->ptr_type = ptr_type;
	param->ptr_size = ptr_size;

	list_add_tail(&param->node, &params_list->node);

	params_list->max_size = max(params_list->max_size, param->ptr_size);
	params_list->max_type = max(params_list->max_type, param->ptr_type);
	params_list->max_h++;

	return 0;
}

static inline struct params_list_info *find_params_list(const char *name)
{
	struct params_list_info *params_list = NULL;
	int idx = 0;

	if (!name) {
		dbg_info("invalid name\n");
		return NULL;
	}

	dbg_dbg("%s\n", name);
	while (!IS_ERR_OR_NULL(params_lists[idx])) {
		params_list = params_lists[idx];
		dbg_dbg("%dth params_list name is %s\n", idx, params_list->name);
		if (STREQ(params_list->name, name))
			return params_list;
		idx++;
		BUG_ON(idx == ARRAY_SIZE(params_lists));
	};

	dbg_info("%s is not exist, so create it\n", name);
	params_list = kzalloc(sizeof(struct params_list_info), GFP_KERNEL);
	params_list->name = kstrdup(name, GFP_KERNEL);

	INIT_LIST_HEAD(&params_list->node);

	params_lists[idx] = params_list;

	return params_list;
}

static struct dentry *debugfs_root;
void init_debugfs_param(const char *name, void *ptr, u32 ptr_type, u32 sum_size, u32 ptr_unit)
{
	struct params_list_info *params_list = find_params_list(name);
	int i = 0;

	if (!name || !ptr || !ptr_type || !sum_size || !params_list) {
		dbg_info("invalid param\n");
		return;
	}

	if (ptr_type != U8_MAX && ptr_type != U32_MAX) {
		dbg_info("ptr_type(%d) invalid\n", ptr_type);
		return;
	}

	if (sum_size > 64) {
		dbg_info("sum_size(%d) invalid\n", sum_size);
		return;
	}

	if (ptr_unit > 64) {
		dbg_info("ptr_unit(%d) invalid\n", ptr_unit);
		return;
	}

	if (ptr_unit == 0) {
		dbg_info("ptr_unit use sum_size(%d)\n", sum_size);
		ptr_unit = sum_size;
	}

	if (!debugfs_root) {
		debugfs_root = debugfs_create_dir("dd_param", NULL);
		debugfs_create_file("_help", 0400, debugfs_root, NULL, &help_fops);
	}

	if (unlikely(list_empty(&params_list->node))) {
		dbg_info("debugfs_create_file for %s\n", name);
		debugfs_create_file(name, 0600, debugfs_root, params_list, &param_fops);
	}

	for (i = 0; i < sum_size; i += ptr_unit) {
		if (ptr_type == U8_MAX)
			add_param(params_list, (u8 *)ptr + i, ptr_type, (i + ptr_unit < sum_size) ? ptr_unit : sum_size - i);
		else if (ptr_type == U32_MAX)
			add_param(params_list, (u32 *)ptr + i, ptr_type, (i + ptr_unit < sum_size) ? ptr_unit : sum_size - i);
	}
}
#endif

