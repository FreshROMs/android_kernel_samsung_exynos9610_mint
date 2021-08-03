/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of.h>

#define SUBFPCB_MAX	3

static int subfpcb_gpio[SUBFPCB_MAX];
static int fpcb_gpio_count;

static int get_subfpcb_gpio(void)
{
	struct device_node *np;
	int i;

	np = of_find_node_by_path("/fpcb_type");

	if (!np) {
		pr_err("%s: there's no fpcb_type node on dtb\n", __func__);
		return -1;
	}

	fpcb_gpio_count = of_gpio_count(np);

	if (fpcb_gpio_count <= 0) {
		pr_err("%s: there's no gpio in fpcb_type node\n", __func__);
		return -1;
	}

	for (i = 0; i < fpcb_gpio_count; i++)
		subfpcb_gpio[i] = of_get_gpio(np, i);

	of_node_put(np);

	for (i = 0; i < fpcb_gpio_count; i++) {
		if (!gpio_is_valid(subfpcb_gpio[i])) {
			pr_err("%s: fpcb gpio is not valid\n", __func__);
			fpcb_gpio_count = 0;
			return -1;
		}
	}

	pr_info("%s: sub fpcb gpios: ", __func__);
	for (i = 0; i < fpcb_gpio_count; i++)
		pr_info("%d ", subfpcb_gpio[i]);
	pr_info("\n");

	return 0;
}

static int check_subfpcb_type(struct seq_file *m, void *v)
{
	int retval = -1;
	int i;

	if (fpcb_gpio_count <= 0)
		goto out;

	/* default output: SUB_REV1/SUB_REV2 */
	retval = gpio_get_value(subfpcb_gpio[0]) << 1;
	retval += gpio_get_value(subfpcb_gpio[1]);

	/* add output: increases from back to front.
				.../SUB_REV3/SUB_REV1/SUB_REV2 */
	if (fpcb_gpio_count >= 3) {
		for (i = 3; i <= fpcb_gpio_count; i++)
			retval += gpio_get_value(subfpcb_gpio[i-1]) << (i-1);
	}

	pr_info("%s: fpcb gpio value is %d\n", __func__, retval);

out:
	seq_printf(m, "%u\n", retval);

	return 0;
}

static int check_subfpcb_type_open(struct inode *inode, struct file *file)
{
	return single_open(file, check_subfpcb_type, NULL);
}

static const struct file_operations check_subfpcb_type_fops = {
	.open		= check_subfpcb_type_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init sec_gpio_detect_init(void)
{
	struct proc_dir_entry *entry;

	pr_info("%s start\n", __func__);

	entry = proc_create("subfpcb_type", S_IRUGO, NULL, &check_subfpcb_type_fops);
	if (!entry)
	{
		pr_err("%s: failed to create a proc fs node\n", __func__);
		return -ENOMEM;
	}

	if (get_subfpcb_gpio())
	{
		pr_err("%s: failed to get gpio node\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

module_init(sec_gpio_detect_init);
