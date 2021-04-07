/* Synaptics Register Mapped Interface (RMI4) I2C Physical Layer Driver.
 * Copyright (c) 2007-2012, Synaptics Incorporated
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/unaligned.h>
#include <asm/siginfo.h>
#include <mach/cpufreq.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/signal.h>
#include <linux/i2c/synaptics_rmi.h>
#include "synaptics_i2c_rmi.h"

#define REG_ADDR_LIMIT 0xFFFF

static ssize_t rmidb_sysfs_data_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t rmidb_sysfs_data_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t rmidb_sysfs_pid_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t rmidb_sysfs_pid_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t rmidb_sysfs_term_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t rmidb_sysfs_address_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t rmidb_sysfs_address_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t rmidb_sysfs_length_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t rmidb_sysfs_length_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t rmidb_sysfs_query_base_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t rmidb_sysfs_control_base_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t rmidb_sysfs_data_base_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t rmidb_sysfs_command_base_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf);

struct rmidb_handle {
	pid_t pid;
	unsigned char intr_mask;
	unsigned char intr_reg_num;
	unsigned short query_base_addr;
	unsigned short control_base_addr;
	unsigned short data_base_addr;
	unsigned short command_base_addr;
	unsigned short address;
	unsigned int length;
	struct siginfo interrupt_signal;
	struct siginfo terminate_signal;
	struct task_struct *task;
	struct synaptics_rmi4_data *rmi4_data;
	struct synaptics_rmi4_exp_fn_ptr *fn_ptr;
	struct kobject *sysfs_dir;
};

static struct bin_attribute attr_data = {
	.attr = {
		.name = "data",
		.mode = (S_IRUGO | S_IWUSR | S_IWGRP),
	},
	.size = 0,
	.read = rmidb_sysfs_data_show,
	.write = rmidb_sysfs_data_store,
};

static struct device_attribute attrs[] = {
	__ATTR(pid, S_IRUGO | S_IWUSR | S_IWGRP,
			rmidb_sysfs_pid_show,
			rmidb_sysfs_pid_store),
	__ATTR(term, S_IWUSR | S_IWGRP,
			synaptics_rmi4_show_error,
			rmidb_sysfs_term_store),
	__ATTR(address, S_IRUGO | S_IWUSR | S_IWGRP,
			rmidb_sysfs_address_show,
			rmidb_sysfs_address_store),
	__ATTR(length, S_IRUGO | S_IWUSR | S_IWGRP,
			rmidb_sysfs_length_show,
			rmidb_sysfs_length_store),
	__ATTR(query_base_addr, S_IRUGO,
			rmidb_sysfs_query_base_addr_show,
			synaptics_rmi4_store_error),
	__ATTR(control_base_addr, S_IRUGO,
			rmidb_sysfs_control_base_addr_show,
			synaptics_rmi4_store_error),
	__ATTR(data_base_addr, S_IRUGO,
			rmidb_sysfs_data_base_addr_show,
			synaptics_rmi4_store_error),
	__ATTR(command_base_addr, S_IRUGO,
			rmidb_sysfs_command_base_addr_show,
			synaptics_rmi4_store_error),
};

static struct rmidb_handle *rmidb;

static ssize_t rmidb_sysfs_data_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
	unsigned int data_length = rmidb->length;

	if (data_length > (REG_ADDR_LIMIT - rmidb->address))
		data_length = REG_ADDR_LIMIT - rmidb->address;

	if (count < data_length) {
		tsp_debug_err(true, &rmidb->rmi4_data->i2c_client->dev,
				"%s: Not enough space (%d bytes) in buffer\n",
				__func__, count);
		return -EINVAL;
	}

	if (data_length) {
		retval = rmidb->fn_ptr->read(rmidb->rmi4_data,
				rmidb->address,
				(unsigned char *)buf,
				data_length);
		if (retval < 0) {
			tsp_debug_err(true, &rmidb->rmi4_data->i2c_client->dev,
					"%s: Failed to read data\n",
					__func__);
			return retval;
		}
	} else {
		return -EINVAL;
	}

	return data_length;
}

static ssize_t rmidb_sysfs_data_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
	unsigned int data_length = rmidb->length;

	if (data_length > (REG_ADDR_LIMIT - rmidb->address))
		data_length = REG_ADDR_LIMIT - rmidb->address;

	if (data_length) {
		retval = rmidb->fn_ptr->write(rmidb->rmi4_data,
				rmidb->address,
				(unsigned char *)buf,
				data_length);
		if (retval < 0) {
			tsp_debug_err(true, &rmidb->rmi4_data->i2c_client->dev,
					"%s: Failed to write data\n",
					__func__);
			return retval;
		}
	} else {
		return -EINVAL;
	}

	return count;
}

static ssize_t rmidb_sysfs_pid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", rmidb->pid);
}

static ssize_t rmidb_sysfs_pid_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	rmidb->pid = input;

	if (rmidb->pid) {
		rmidb->task = pid_task(find_vpid(rmidb->pid), PIDTYPE_PID);
		if (!rmidb->task) {
			tsp_debug_err(true, &rmidb->rmi4_data->i2c_client->dev,
					"%s: Failed to locate debug app PID\n",
					__func__);
			return -EINVAL;
		}
	}

	return count;
}

static ssize_t rmidb_sysfs_term_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input != 1)
		return -EINVAL;

	if (rmidb->pid)
		send_sig_info(SIGTERM, &rmidb->terminate_signal, rmidb->task);

	return count;
}

static ssize_t rmidb_sysfs_address_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%04x\n", rmidb->address);
}

static ssize_t rmidb_sysfs_address_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input > REG_ADDR_LIMIT)
		return -EINVAL;

	rmidb->address = (unsigned short)input;

	return count;
}

static ssize_t rmidb_sysfs_length_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", rmidb->length);
}

static ssize_t rmidb_sysfs_length_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input > REG_ADDR_LIMIT)
		return -EINVAL;

	rmidb->length = input;

	return count;
}

static ssize_t rmidb_sysfs_query_base_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%04x\n", rmidb->query_base_addr);
}

static ssize_t rmidb_sysfs_control_base_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%04x\n", rmidb->control_base_addr);
}

static ssize_t rmidb_sysfs_data_base_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%04x\n", rmidb->data_base_addr);
}

static ssize_t rmidb_sysfs_command_base_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%04x\n", rmidb->command_base_addr);
}

static int rmidb_scan_pdt(void)
{
	int retval;
	unsigned short ii;
	unsigned char page;
	unsigned char intr_count = 0;
	unsigned char intr_offset;
	bool fdb_found = false;
	struct synaptics_rmi4_fn_desc rmi_fd;

	for (page = 0; page < PAGES_TO_SERVICE; page++) {
		for (ii = PDT_START; ii > PDT_END; ii -= PDT_ENTRY_SIZE) {
			ii |= (page << 8);

			retval = rmidb->fn_ptr->read(rmidb->rmi4_data,
					ii,
					(unsigned char *)&rmi_fd,
					sizeof(rmi_fd));
			if (retval < 0)
				return retval;

			if (!rmi_fd.fn_number)
				break;

			if (rmi_fd.fn_number == SYNAPTICS_RMI4_FDB) {
				fdb_found = true;
				goto fdb_found;
			}

			intr_count += (rmi_fd.intr_src_count & MASK_3BIT);
		}
	}

	if (!fdb_found) {
		tsp_debug_err(true, &rmidb->rmi4_data->i2c_client->dev,
				"%s: Failed to find FDB\n",
				__func__);
		return 0;
	}

fdb_found:
	rmidb->query_base_addr = rmi_fd.query_base_addr | (page << 8);
	rmidb->control_base_addr = rmi_fd.ctrl_base_addr | (page << 8);
	rmidb->data_base_addr = rmi_fd.data_base_addr | (page << 8);
	rmidb->command_base_addr = rmi_fd.cmd_base_addr | (page << 8);

	rmidb->intr_reg_num = (intr_count + 7) / 8;
	if (rmidb->intr_reg_num != 0)
		rmidb->intr_reg_num -= 1;

	rmidb->intr_mask = 0;
	intr_offset = intr_count % 8;
	for (ii = intr_offset;
			ii < ((rmi_fd.intr_src_count & MASK_3BIT) +
				intr_offset);
			ii++) {
		rmidb->intr_mask |= 1 << ii;
	}

	return 0;
}

static void rmidb_attn(struct synaptics_rmi4_data *rmi4_data,
		unsigned char intr_mask)
{
	if (!rmidb)
		return;

	if (rmidb->pid && (rmidb->intr_mask & intr_mask))
		send_sig_info(SIGIO, &rmidb->interrupt_signal, rmidb->task);

	return;
}

static int rmidb_init(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char attr_count;
	int attr_count_num;

	rmidb = kzalloc(sizeof(*rmidb), GFP_KERNEL);
	if (!rmidb) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for rmidb\n",
				__func__);
		retval = -ENOMEM;
		goto err_rmidb;
	}

	rmidb->fn_ptr =  kzalloc(sizeof(*(rmidb->fn_ptr)), GFP_KERNEL);
	if (!rmidb->fn_ptr) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for fn_ptr\n",
				__func__);
		retval = -ENOMEM;
		goto err_fn_ptr;
	}

	memset(&rmidb->interrupt_signal, 0, sizeof(rmidb->interrupt_signal));
	rmidb->interrupt_signal.si_signo = SIGIO;
	rmidb->interrupt_signal.si_code = SI_USER;

	memset(&rmidb->terminate_signal, 0, sizeof(rmidb->terminate_signal));
	rmidb->terminate_signal.si_signo = SIGTERM;
	rmidb->terminate_signal.si_code = SI_USER;

	rmidb->fn_ptr->read = rmi4_data->i2c_read;
	rmidb->fn_ptr->write = rmi4_data->i2c_write;
	rmidb->fn_ptr->enable = rmi4_data->irq_enable;
	rmidb->rmi4_data = rmi4_data;

	retval = rmidb_scan_pdt();
	if (retval < 0) {
		retval = 0;
		goto err_scan_pdt;
	}

	rmidb->sysfs_dir = kobject_create_and_add("rmidb",
			&rmi4_data->input_dev->dev.kobj);
	if (!rmidb->sysfs_dir) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to create sysfs directory\n",
				__func__);
		retval = -ENODEV;
		goto err_sysfs_dir;
	}

	retval = sysfs_create_bin_file(rmidb->sysfs_dir,
			&attr_data);
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to create sysfs bin file\n",
				__func__);
		goto err_sysfs_bin;
	}

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = sysfs_create_file(rmidb->sysfs_dir,
				&attrs[attr_count].attr);
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->input_dev->dev,
					"%s: Failed to create sysfs attributes\n",
					__func__);
			retval = -ENODEV;
			goto err_sysfs_attrs;
		}
	}

	return 0;

err_sysfs_attrs:
	attr_count_num = (int)attr_count;
	for (attr_count_num--; attr_count_num >= 0; attr_count_num--)
		sysfs_remove_file(rmidb->sysfs_dir, &attrs[attr_count].attr);

	sysfs_remove_bin_file(rmidb->sysfs_dir, &attr_data);

err_sysfs_bin:
	kobject_put(rmidb->sysfs_dir);

err_sysfs_dir:
err_scan_pdt:
	kfree(rmidb->fn_ptr);

err_fn_ptr:
	kfree(rmidb);
	rmidb = NULL;

err_rmidb:
	return retval;
}

static void rmidb_remove(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned char attr_count;

	if (!rmidb)
		goto exit;

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++)
		sysfs_remove_file(rmidb->sysfs_dir, &attrs[attr_count].attr);

	sysfs_remove_bin_file(rmidb->sysfs_dir, &attr_data);

	kobject_put(rmidb->sysfs_dir);

	kfree(rmidb->fn_ptr);
	kfree(rmidb);
	rmidb = NULL;

exit:
	return;
}

int rmidb_module_register(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;

	retval = synaptics_rmi4_new_function(RMI_DB,
			rmi4_data,
			rmidb_init,
			NULL,
			rmidb_remove,
			rmidb_attn);

	return retval;
}
