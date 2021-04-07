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
#include <linux/sort.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/backlight.h>

#include "dd.h"

/*
 * bl_tuning usage
 * you can skip out if your model has no outdoor or hbm
 * you can skip platform_brightness. if you skip it, we reuse binary default value
 * : (colon) is delemeter to separate tune_value and platform platform_brightness
 * echo min dft max > bl_tuning
 * echo min dft max out > bl_tuning
 * echo min dft max: platform_min platform_dft platform_max > bl_tuning
 * echo min dft max out: platform_min platform_dft platform_max platform_out > bl_tuning
 * echo 0 > bl_tuning
 *
 * ex) echo 1 2 3 > /d/dd_backlight/bl_tuning
 * ex) echo 1 2 3 4 > /d/dd_backlight/bl_tuning
 * ex) echo 1 2 3: 4 5 6 > /d/dd_backlight/bl_tuning
 * ex) echo 1 2 3 4: 5 6 7 8 > /d/dd_backlight/bl_tuning = this means 1 2 3 4 for tune_value and 5 6 7 8 for brightness
 * ex) echo 0 > /d/dd_backlight/bl_tuning = this means reset brightness table to default
 *
 * ex) echo 1 2 3 4: 5 6 7 > /d/dd_backlight/bl_tuning (X) because tune_value(1 2 3 4) part count is 4 but brightness(5 6 7) part count is 3
 * ex) echo 1 2 3 4: 5 6 7: 8 9 10 > /d/dd_backlight/bl_tuning (X) because we allow only one :(colon) to separate platform brightness point
 *
 */

/* ic tuning usage
 * echo addr (value. if you skip it, we regard this is read mode) > /d/dd_backlight/ic_tuning-i2c_client->name

 * ex) echo 0x1 > /d/dd_backlight/ic_tuning-i2c_client
 * = this means you assign read i2c_command as 0x1 when you cat this sysfs next time
 * ex) echo 0x1 0x2 > /d/dd_backlight/ic_tuning-i2c_client
 * = this means you write to 0x01(i2c_command) with 0x02(value) and you assign read i2c_command as 0x1 when you cat this sysfs next time
 */

#define dbg_info(fmt, ...)	pr_info(pr_fmt("%s: %3d: %s: " fmt), "lcd panel", __LINE__, __func__, ##__VA_ARGS__)
#define dbg_warn(fmt, ...)	pr_warn(pr_fmt("%s: %3d: %s: " fmt), "lcd panel", __LINE__, __func__, ##__VA_ARGS__)

#define BL_POINTS	\
	X(OUT)		\
	X(MAX)		\
	X(DFT)		\
	X(MIN)		\
	X(OFF)		\
	X(END)

#define X(a)	BL_POINT_##a,
enum {	BL_POINTS	};
#undef X

#define X(a)	#a,
static char *BL_POINT_NAME[] = { BL_POINTS };
#undef X

#define INPUT_LIMIT	80
#define MAX_I2C_CLIENT	5

struct bl_info {
	struct backlight_device *bd;
	unsigned int *brightness_table;
	unsigned int *brightness_reset;

	unsigned int *default_tune_value;
	unsigned int *default_brightness;

	unsigned int input_tune_value[INPUT_LIMIT];
	unsigned int input_brightness[INPUT_LIMIT];
};

struct ic_info {
	struct list_head client_node;
	struct backlight_device *bd;
	struct i2c_client *client;
	unsigned int command;
	int (*i2c_cmd)(struct i2c_client *client, unsigned int num, void *arg);

	char *i2c_debugfs_name;
};

static LIST_HEAD(client_list);

static void make_bl_default_point(struct bl_info *bl)
{
	int i;

	if (IS_ERR_OR_NULL(bl->default_brightness)) {
		bl->default_brightness = kcalloc(BL_POINT_END, sizeof(unsigned int), GFP_KERNEL);

		bl->default_brightness[BL_POINT_OFF] = 0;
		bl->default_brightness[BL_POINT_MIN] = 1;
		bl->default_brightness[BL_POINT_DFT] = bl->bd->props.brightness;
		bl->default_brightness[BL_POINT_MAX] = 255;
		bl->default_brightness[BL_POINT_OUT] = bl->bd->props.max_brightness;

		for (i = 0; i <= bl->default_brightness[BL_POINT_MAX]; i++) {
			if (bl->brightness_table[i]) {
				bl->default_brightness[BL_POINT_MIN] = i;
				break;
			}
		}
	}

	if (IS_ERR_OR_NULL(bl->default_tune_value)) {
		bl->default_tune_value = kcalloc(BL_POINT_END, sizeof(unsigned int), GFP_KERNEL);

		for (i = 0; i < BL_POINT_END; i++)
			bl->default_tune_value[i] = bl->brightness_table[bl->default_brightness[i]];
	}
}

static void reverse_order(unsigned int *o, unsigned int size)
{
	unsigned int a = 0, b = size - 1, t;

	if (!size || !b)
		return;

	while (a < b) {
		t = o[a];
		o[a] = o[b];
		o[b] = t;

		a++;
		b--;
	}
}

static unsigned int parse_curve(char *str, char *delim, unsigned int *out)
{
	unsigned int i = 0;
	char *p = NULL;
	int ret;

	if (!str) {
		dbg_info("str is invalid. null\n");
		return i;
	}

	/* trim */
	for (i = 0; str[i]; i++) {
		if (isdigit(str[i]))
			break;
	}
	str = str + i;

	/* separate with delimiter */
	i = 0;
	while ((p = strsep(&str, delim)) != NULL) {
		if (*p == '\0')
			continue;
		ret = kstrtouint(p, 0, out + i);
		if (ret < 0)
			break;
		i++;
	}

	reverse_order(out, i);

	return i;
}

static int make_bl_curve(struct bl_info *bl, unsigned int *tune_value_point, unsigned int *brightness_point)
{
	int i, idx;
	unsigned int value;

	for (i = 0; i <= brightness_point[0]; i++) {
		for (idx = 0; brightness_point[idx]; idx++) {
			if (i >= brightness_point[idx])
				break;
		}

		if ((i >= 255 && idx == 1) || tune_value_point[idx] == 0)	/* flat */
			value = tune_value_point[idx];
		else if (i >= brightness_point[idx])
			value = (i - brightness_point[idx]) * (tune_value_point[idx - 1] - tune_value_point[idx]) / (brightness_point[idx - 1] - brightness_point[idx]) + tune_value_point[idx];
		else
			value = 0;

		dbg_info("[%4d] = %4d -> %4d, idx: %d,\t%s\n", i, bl->brightness_table[i], value, idx, (value != bl->brightness_table[i]) ? "X" : "");
		bl->brightness_table[i] = value;
	}

	memset(bl->input_tune_value, 0, sizeof(bl->input_tune_value));
	memset(bl->input_brightness, 0, sizeof(bl->input_brightness));

	for (i = 0; brightness_point[i]; i++) {
		bl->input_tune_value[i] = tune_value_point[i];
		bl->input_brightness[i] = brightness_point[i];
	}

	return 0;
}

static int check_curve(struct bl_info *bl, char *str)
{
	int i, ret = 0;
	unsigned int brightness_point[INPUT_LIMIT] = {0, };
	unsigned int tune_value_point[INPUT_LIMIT] = {0, };
	int max_brightness_point = 0;
	int max_tune_value_point = 0;
	int off_bl_point = BL_POINT_END - 1;
	char *pos = NULL;
	char print_buf[INPUT_LIMIT] = {0, };

	struct seq_file m = {
		.buf = print_buf,
		.size = sizeof(print_buf) - 1,
	};

	pos = strrchr(str, ':');
	if (pos) {
		dbg_info("we found colon(:) for brightness level\n");
		*pos = '\0';
		pos++;
	}

	max_tune_value_point = parse_curve(str, " ", tune_value_point);
	max_brightness_point = parse_curve(pos, " ", brightness_point);

	if (bl->bd->props.max_brightness <= 255)
		off_bl_point -= 1;

	dbg_info("max_tune_value_point: %d, max_brightness_point: %d, total_bl_point: %d\n", max_tune_value_point, max_brightness_point, off_bl_point);

	m.count = 0;
	memset(print_buf, 0, sizeof(print_buf));
	for (i = max_tune_value_point - 1; max_tune_value_point && i >= 0; i--)
		seq_printf(&m, "%4d, ", tune_value_point[i]);
	dbg_info("max_tune_value_point: %d, %s\n", max_tune_value_point, m.buf);

	m.count = 0;
	memset(print_buf, 0, sizeof(print_buf));
	for (i = max_brightness_point - 1; max_brightness_point && i >= 0; i--)
		seq_printf(&m, "%4d, ", brightness_point[i]);
	dbg_info("max_brightness_point: %d, %s\n", max_brightness_point, m.buf);

	if (max_brightness_point && (max_tune_value_point != max_brightness_point)) {
		dbg_info("max_tune_value_point: %d, max_brightness_point: %d, mismatch\n", max_tune_value_point, max_brightness_point);
		dbg_info("if you want to change platform brightness point,\n");
		dbg_info("please input same number of tune_value_point vs. brightness_point\n");
		ret = -EINVAL;
		goto exit;
	}

	if (!max_brightness_point && max_tune_value_point != off_bl_point && tune_value_point[max_tune_value_point - 1]) {
		dbg_info("if you want to input max_tune_value_point(%d) different with total_bl_point(%d),\n", max_tune_value_point, off_bl_point);
		dbg_info("please assign platform brightness point together because we can not recognize it automatically\n");
		ret = -EINVAL;
		goto exit;
	}

	if (!max_brightness_point && max_tune_value_point != off_bl_point && !tune_value_point[max_tune_value_point - 1]) {
		dbg_info("input has tune_value including zero(0) point without change platform brightness point,\n");
		dbg_info("so we use default brightness_point\n");
		for (i = 0; i <= off_bl_point; i++)
			brightness_point[i] = bl->default_brightness[i];

		max_tune_value_point = max_brightness_point = off_bl_point;
	}

	if (!max_brightness_point && max_tune_value_point == off_bl_point) {
		dbg_info("we guess you want to change tune_value without change platform brightness point\n");
		dbg_info("so we use default brightness_point\n");
		for (i = 0; i <= off_bl_point; i++)
			brightness_point[i] = bl->default_brightness[i];

		max_brightness_point = off_bl_point;
	}

	if (!max_tune_value_point || !max_brightness_point) {
		dbg_info("max_tune_value_point: %d, max_brightness_point: %d, should not be zero\n", max_tune_value_point, max_brightness_point);
		ret = -EINVAL;
		goto exit;
	}

	if (brightness_point[0] > bl->bd->props.max_brightness) {
		dbg_info("top_brightness_point: %d, it should be smaller than %d\n", brightness_point[0], bl->bd->props.max_brightness);
		ret = -EINVAL;
		goto exit;
	}

	dbg_info("max_tune_value_point: %d, max_brightness_point: %d, off_bl_point: %d\n", max_tune_value_point, max_brightness_point, off_bl_point);

	for (i = max_tune_value_point; i >= 0; i--)
		dbg_info("tune_value_point: %4d, brightness_point: %4d\n", tune_value_point[i], brightness_point[i]);

	make_bl_curve(bl, tune_value_point, brightness_point);

exit:
	return ret;
}

static unsigned int count_char(char *str, char c)
{
	unsigned int i, count = 0;

	for (i = 0; str[i]; i++) {
		if (str[i] == c)
			count++;
	}

	return count;
}

static int bl_tuning_show(struct seq_file *m, void *unused)
{
	struct bl_info *bl = m->private;
	int i, off = 0, point_idx = 0;
	int end = (bl->default_brightness[BL_POINT_MAX] == bl->default_brightness[BL_POINT_OUT]) ? BL_POINT_MAX : BL_POINT_OUT;
	unsigned int *brightness_point;

	for (off = 0; off < INPUT_LIMIT; off++) {
		if (!bl->input_brightness[off]) {
			dbg_info("off: %4d\n", off);
			break;
		}
	};
	off = (off == INPUT_LIMIT) ? 0 : off;

	point_idx = off ? off : BL_POINT_OFF;
	brightness_point = off ? bl->input_brightness : bl->default_brightness;

	seq_puts(m, "TABLE 1-------------------------------------------\n");
	for (i = 0; i <= bl->bd->props.max_brightness; i++)
		seq_printf(m, "[%4d] = %4d\n", i, bl->brightness_table[i]);

	seq_puts(m, "COPY FROM HERE------------------------------------\n");
	seq_puts(m, "DEFAULT ------------------------------------------\n");
	seq_printf(m, "%8s| %8s| %8s\n", " ", "tune", "platform");
	for (i = BL_POINT_OFF; i >= end; i--) {
		seq_printf(m, "%8s| %8d| %8d\n", BL_POINT_NAME[i], bl->default_tune_value[i], bl->default_brightness[i]);
	};

	if (off) {
		seq_puts(m, "TUNING -------------------------------------------\n");
		for (i = off; i >= 0; i--)
			seq_printf(m, "%8s| %8d| %8d\n", " ", bl->input_tune_value[i], bl->input_brightness[i]);
	}

	seq_puts(m, "TABLE 2-------------------------------------------\n");
	for (i = 0; i <= bl->bd->props.max_brightness; i++) {
		seq_printf(m, "%d,", bl->brightness_table[i]);

		if (i && !(i % 10) && point_idx >= 0 && i >= brightness_point[point_idx] && bl->brightness_table[brightness_point[point_idx]]) {
			seq_printf(m, " /* %d: %d */\n", brightness_point[point_idx], bl->brightness_table[brightness_point[point_idx]]);
			point_idx--;
		} else if (!(i % 10) && point_idx >= 0 && i >= brightness_point[point_idx]) {
			seq_puts(m, "\n");
			point_idx--;
		} else if (!(i % 10)) {
			seq_puts(m, "\n");
		} else {
			seq_puts(m, " ");
		}
	}
	seq_puts(m, "\n");

	return 0;
}

static int bl_tuning_open(struct inode *inode, struct file *f)
{
	return single_open(f, bl_tuning_show, inode->i_private);
}

static ssize_t bl_tuning_write(struct file *f, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct bl_info *bl = ((struct seq_file *)f->private_data)->private;
	char ibuf[INPUT_LIMIT] = {0, };
	int ret, i;

	ret = dd_simple_write_to_buffer(ibuf, sizeof(ibuf), ppos, user_buf, count);
	if (ret < 0) {
		dbg_info("dd_simple_write_to_buffer fail: %d\n", ret);
		goto exit;
	}

	if (!strncmp(ibuf, "0", count - 1)) {
		dbg_info("input is 0(zero). reset brightness table to default\n");
		make_bl_curve(bl, bl->default_tune_value, bl->default_brightness);
		for (i = 0; i < bl->bd->props.max_brightness; i++) {
			dbg_info("[%4d] = %4d, %4d%s\n", i, bl->brightness_table[i], bl->brightness_reset[i],
				(bl->brightness_table[i] == bl->brightness_reset[i]) ? "" : ", (X)");
			if (bl->brightness_table[i] != bl->brightness_reset[i]) {
				memcpy(bl->brightness_table, bl->brightness_reset, bl->bd->props.max_brightness * sizeof(unsigned int));
				break;
			}
		}
		memset(bl->input_tune_value, 0, sizeof(bl->input_tune_value));
		memset(bl->input_brightness, 0, sizeof(bl->input_brightness));
		goto exit;
	}

	if (count_char(ibuf, ' ') == 0) {
		dbg_info("input(blank count) is invalid, %d\n", count_char(ibuf, ' '));
		goto exit;
	}

	if (count_char(ibuf, ':') >= 2) {
		dbg_info("input(comma count) is invalid, %d\n", count_char(ibuf, ':'));
		goto exit;
	}

	check_curve(bl, ibuf);

exit:
	return count;
}

static const struct file_operations bl_tuning_fops = {
	.open		= bl_tuning_open,
	.write		= bl_tuning_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int ic_tuning_show(struct seq_file *m, void *unused)
{
	struct ic_info *ic = m->private;
	int ret;

	if (ic->bd->props.fb_blank != FB_BLANK_UNBLANK) {
		dbg_info("fb_blank is invalid, %d\n", ic->bd->props.fb_blank);
		seq_printf(m, "fb_blank is invalid, %d\n", ic->bd->props.fb_blank);
		return 0;
	}

	ret = ic->i2c_cmd ? ic->i2c_cmd(ic->client, I2C_M_RD, &ic->command) : i2c_smbus_read_byte_data(ic->client, ic->command);
	if (ret < 0)
		seq_printf(m, "%02x, i2c_rx errno: %d\n", ic->command, ret);
	else
		seq_printf(m, "%02x, i2c_rx %02x\n", ic->command, ret);

	return 0;
}

static int ic_tuning_open(struct inode *inode, struct file *f)
{
	return single_open(f, ic_tuning_show, inode->i_private);
}

static ssize_t ic_tuning_write(struct file *f, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct ic_info *ic = ((struct seq_file *)f->private_data)->private;
	int ret = 0, command = 0, value = 0;
	char ibuf[INPUT_LIMIT] = {0, };
	struct i2c_msg xfer[2] = {{0, }, {0, } };
	u8 i2c_wbuf[3] = {0, };
	u8 i2c_rbuf[1] = {0, };

	if (ic->bd->props.fb_blank != FB_BLANK_UNBLANK) {
		dbg_info("fb_blank is invalid, %d\n", ic->bd->props.fb_blank);
		goto exit;
	}

	ret = dd_simple_write_to_buffer(ibuf, sizeof(ibuf), ppos, user_buf, count);
	if (ret < 0) {
		dbg_info("dd_simple_write_to_buffer fail: %d\n", ret);
		goto exit;
	}

	ret = sscanf(ibuf, "%8x %8x", &command, &value);
	if (clamp(ret, 1, 2) != ret) {
		dbg_info("input is invalid, %d\n", ret);
		goto exit;
	}

	if (clamp(command, 0, INT_MAX) != command) {
		dbg_info("input is invalid, command: %02x\n", command);
		goto exit;
	}

	if (clamp(value, 0, INT_MAX) != value) {
		dbg_info("input is invalid, value: %02x\n", value);
		goto exit;
	}

	dbg_info("command: %02x, value: %02x%s\n", command, value, (ret == 2) ? ", tx_mode" : "");

	ic->command = command;

	if (command > U8_MAX) {
		i2c_wbuf[0] = (command & 0xff00) >> 8;
		i2c_wbuf[1] = (command & 0x00ff);
		i2c_wbuf[2] = value;

		xfer[0].addr = ic->client->addr;
		xfer[0].flags = I2C_M_TEN;
		xfer[0].len = 3;
		xfer[0].buf = i2c_wbuf;

		xfer[1].addr = ic->client->addr;
		xfer[1].flags = I2C_M_TEN | I2C_M_RD;
		xfer[1].len = ARRAY_SIZE(i2c_rbuf);
		xfer[1].buf = i2c_rbuf;
	} else {
		i2c_wbuf[0] = (command & 0x00ff);
		i2c_wbuf[1] = value;

		xfer[0].addr = ic->client->addr;
		xfer[0].flags = 0;
		xfer[0].len = 2;
		xfer[0].buf = i2c_wbuf;

		xfer[1].addr = ic->client->addr;
		xfer[1].flags = I2C_M_RD;
		xfer[1].len = ARRAY_SIZE(i2c_rbuf);
		xfer[1].buf = i2c_rbuf;
	}

	/* tx */
	if (ret == 2) {
		ret = ic->i2c_cmd ? ic->i2c_cmd(ic->client, 1, (void *)&xfer) : i2c_smbus_write_byte_data(ic->client, ic->command, value);
		if (ret < 0)
			dbg_info("%02x, i2c_tx errno: %d\n", command, ret);
	}

	/* rx */
	ret = ic->i2c_cmd ? ic->i2c_cmd(ic->client, 2, (void *)&xfer) : i2c_smbus_read_byte_data(ic->client, ic->command);
	if (ret < 0)
		dbg_info("%02x, i2c_rx errno: %d\n", command, ret);
	else
		dbg_info("%02x, i2c_rx: %02x\n", command, ret);

exit:
	return count;
}

static const struct file_operations ic_tuning_fops = {
	.open		= ic_tuning_open,
	.write		= ic_tuning_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int help_show(struct seq_file *m, void *unused)
{
	struct bl_info *bl = m->private;
	int i = 0;
	int end = (bl->default_brightness[BL_POINT_MAX] == bl->default_brightness[BL_POINT_OUT]) ? BL_POINT_MAX : BL_POINT_OUT;
	struct ic_info *ic = NULL;

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
	seq_puts(m, "# cd /d/dd_backlight\n");
	seq_puts(m, "\n");
	seq_puts(m, "---------- bl_tuning usage\n");
	if (end == BL_POINT_OUT) {
		seq_puts(m, "# echo min def max out > bl_tuning\n");
		seq_puts(m, "# echo min def max out: platform min def max out > bl_tuning\n");
	} else {
		seq_puts(m, "# echo min def max > bl_tuning\n");
		seq_puts(m, "# echo min def max: platform min def max > bl_tuning\n");
	}
	seq_puts(m, "# echo 0 > bl_tuning\n");
	seq_puts(m, "# cat bl_tuning\n");
	seq_puts(m, "1. 'echo 0' is for reset brightness table to default\n");
	seq_puts(m, "2. 'cat bl_tuning' is for check latest brightness tuning table\n");
	seq_puts(m, "3. colon(:) is delimiter to separate platform brightness. optional\n");
	seq_printf(m, "4. you can not change total platform brightness range(0~%d)\n", bl->default_brightness[end]);
	seq_puts(m, "ex) # ");
	for (i = BL_POINT_MIN; i >= end; i--)
		seq_printf(m, "%d ", bl->default_tune_value[i]);
	seq_puts(m, "> bl_tuning\n");
	seq_puts(m, "ex) # ");
	for (i = BL_POINT_MIN; i >= end; i--)
		seq_printf(m, "%d ", bl->default_tune_value[i]);
	seq_puts(m, ": ");
	for (i = BL_POINT_MIN; i >= end; i--)
		seq_printf(m, "%d ", bl->default_brightness[i]);
	seq_puts(m, "> bl_tuning\n");
	seq_puts(m, "ex) # echo 0 > bl_tuning\n");
	seq_puts(m, "ex) # cat bl_tuning\n");
	seq_puts(m, "\n");

	if (unlikely(list_empty(&client_list)))
		return 0;
	ic = list_first_entry(&client_list, struct ic_info, client_node);
	seq_puts(m, "---------- ic_tuning usage\n");
	seq_printf(m, "# echo i2c_command > %s\n", ic->i2c_debugfs_name);
	seq_printf(m, "# echo i2c_command value > %s\n", ic->i2c_debugfs_name);
	seq_puts(m, "\n");

	list_for_each_entry(ic, &client_list, client_node) {
		seq_printf(m, "ex) # echo 3 > %s\n", ic->i2c_debugfs_name);
		seq_printf(m, "ex) # cat %s\n", ic->i2c_debugfs_name);
		seq_printf(m, "= get read result from 0x03(i2c_command) of %s\n", ic->i2c_debugfs_name);
		seq_printf(m, "ex) # echo 3 ff > %s\n", ic->i2c_debugfs_name);
		seq_printf(m, "= write 0xff(value) to 0x03(i2c_command) of %s\n", ic->i2c_debugfs_name);
		seq_printf(m, "ex) # cat %s\n", ic->i2c_debugfs_name);
		seq_printf(m, "= get read result from 0x03(i2c_command) of %s\n", ic->i2c_debugfs_name);
	}
	seq_puts(m, "\n");

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

static struct dentry *debugfs_root;
int init_debugfs_backlight(struct backlight_device *bd, unsigned int *table, struct i2c_client **clients)
{
	struct bl_info *bl = NULL;
	struct ic_info *ic = NULL;
	int ret = 0, i2c_count;
	char name_string[] = "ic_tuning-";
	char full_string[I2C_NAME_SIZE + (u16)(ARRAY_SIZE(name_string))];
	struct i2c_driver *driver;

	if (!bd) {
		dbg_warn("failed to get backlight_device\n");
		ret = -ENODEV;
		goto exit;
	}

	if (!table && !clients) {
		dbg_warn("failed to get backlight table and client\n");
		goto exit;
	}

	dbg_info("+\n");

	if (!debugfs_root) {
		debugfs_root = debugfs_create_dir("dd_backlight", NULL);

		bl = kzalloc(sizeof(struct bl_info), GFP_KERNEL);
		bl->bd = bd;
		bl->brightness_table = table ? table : kcalloc(bd->props.max_brightness, sizeof(unsigned int), GFP_KERNEL);
		bl->brightness_reset = kmemdup(bl->brightness_table, bd->props.max_brightness * sizeof(unsigned int), GFP_KERNEL);
		make_bl_default_point(bl);

		debugfs_create_file("_help", 0400, debugfs_root, bl, &help_fops);
		debugfs_create_file("bl_tuning", 0600, debugfs_root, bl, &bl_tuning_fops);
	}

	for (i2c_count = 0; i2c_count < MAX_I2C_CLIENT && clients && clients[i2c_count]; i2c_count++) {
		list_for_each_entry(ic, &client_list, client_node) {
			if (ic->client == clients[i2c_count]) {
				dbg_info("%s %s already exist\n", dev_name(&clients[i2c_count]->adapter->dev), dev_name(&clients[i2c_count]->dev));
				goto exit;
			}
		}

		ic = kzalloc(sizeof(struct ic_info), GFP_KERNEL);

		memset(full_string, 0, sizeof(full_string));
		scnprintf(full_string, sizeof(full_string), "%s%s", name_string, clients[i2c_count]->name);
		debugfs_create_file(full_string, 0600, debugfs_root, ic, &ic_tuning_fops);

		ic->bd = bd;
		ic->client = clients[i2c_count];

		driver = to_i2c_driver(ic->client->dev.driver);
		ic->i2c_cmd = driver->command ? driver->command : NULL;

		dbg_info("%s %s %s\n", full_string, dev_name(&clients[i2c_count]->adapter->dev), dev_name(&clients[i2c_count]->dev));

		ic->i2c_debugfs_name = kstrdup(full_string, GFP_KERNEL);

		INIT_LIST_HEAD(&ic->client_node);
		list_add_tail(&ic->client_node, &client_list);
	}

	dbg_info("-\n");

exit:
	return ret;
}
#endif

