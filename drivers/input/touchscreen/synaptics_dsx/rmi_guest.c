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
#include "synaptics_i2c_rmi.h"

#include "Multiverse/GMvSystem.h"

#define GEUST_PAGES_TO_SERVICE (0x60)
#define HARDCODE_PDT

static ssize_t rmi_guest_sysfs_query_base_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t rmi_guest_sysfs_control_base_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t rmi_guest_sysfs_data_base_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t rmi_guest_sysfs_command_base_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf);

struct rmi_guest_handle {
	unsigned char intr_mask;
	unsigned char intr_reg_num;
	unsigned short query_base_addr;
	unsigned short control_base_addr;
	unsigned short data_base_addr;
	unsigned short command_base_addr;
	struct synaptics_rmi4_data *rmi4_data;
	struct synaptics_rmi4_exp_fn_ptr *fn_ptr;
	struct kobject *sysfs_dir;
};

static struct device_attribute attrs[] = {
	__ATTR(query_base_addr, S_IRUGO,
			rmi_guest_sysfs_query_base_addr_show,
			synaptics_rmi4_store_error),
	__ATTR(control_base_addr, S_IRUGO,
			rmi_guest_sysfs_control_base_addr_show,
			synaptics_rmi4_store_error),
	__ATTR(data_base_addr, S_IRUGO,
			rmi_guest_sysfs_data_base_addr_show,
			synaptics_rmi4_store_error),
	__ATTR(command_base_addr, S_IRUGO,
			rmi_guest_sysfs_command_base_addr_show,
			synaptics_rmi4_store_error),
};

static struct rmi_guest_handle *rmiguest;

static ssize_t rmi_guest_sysfs_query_base_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%04x\n",
			rmiguest->query_base_addr);
}

static ssize_t rmi_guest_sysfs_control_base_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%04x\n",
			rmiguest->control_base_addr);
}

static ssize_t rmi_guest_sysfs_data_base_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%04x\n",
			rmiguest->data_base_addr);
}

static ssize_t rmi_guest_sysfs_command_base_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%04x\n",
			rmiguest->command_base_addr);
}

static int rmi_guest_scan_pdt(void)
{
	int retval;
	unsigned short ii;
	unsigned short addr;
	unsigned char page;
	unsigned char intr_count = 0;
	unsigned char intr_offset;
	bool fguest_found = false;
	struct synaptics_rmi4_fn_desc rmi_fd;
	struct synaptics_rmi4_data *rmi4_data = rmiguest->rmi4_data;

	for (page = 0; page <= GEUST_PAGES_TO_SERVICE; page++) {
		for (ii = PDT_START; ii > PDT_END; ii -= PDT_ENTRY_SIZE) {
			ii |= (page << 8);

			retval = rmiguest->fn_ptr->read(rmiguest->rmi4_data,
					ii,
					(unsigned char *)&rmi_fd,
					sizeof(rmi_fd));
			if (retval < 0)
				return retval;

			ii &= ~(MASK_8BIT << 8);

			if (!rmi_fd.fn_number)
				break;

			if (rmi_fd.fn_number == SYNAPTICS_RMI4_F60) {
				fguest_found = true;
				goto fguest_found;
			}

			intr_count += (rmi_fd.intr_src_count & MASK_3BIT);
		}
	}

#ifndef HARDCODE_PDT
	if (!fguest_found) {
		tsp_debug_err(true, &rmiguest->rmi4_data->i2c_client->dev,
				"%s: Failed to find F60\n",
				__func__);
		return -EINVAL;
	}
#endif

fguest_found:
	rmiguest->query_base_addr = rmi_fd.query_base_addr | (page << 8);
	rmiguest->control_base_addr = rmi_fd.ctrl_base_addr | (page << 8);
	rmiguest->data_base_addr = rmi_fd.data_base_addr | (page << 8);
	rmiguest->command_base_addr = rmi_fd.cmd_base_addr | (page << 8);

#ifdef HARDCODE_PDT
	rmiguest->query_base_addr = 0x0000;
	rmiguest->control_base_addr = 0x0000;
	rmiguest->data_base_addr = 0x6000;
	rmiguest->command_base_addr = 0x0000;
	rmi_fd.intr_src_count = 1;
#endif

	rmiguest->intr_reg_num = (intr_count + 7) / 8;
	if (rmiguest->intr_reg_num != 0)
		rmiguest->intr_reg_num -= 1;

	rmiguest->intr_mask = 0;
	intr_offset = intr_count % 8;
	for (ii = intr_offset;
			ii < ((rmi_fd.intr_src_count & MASK_3BIT) +
			intr_offset);
			ii++) {
		rmiguest->intr_mask |= 1 << ii;
	}

	rmi4_data->intr_mask[0] |= rmiguest->intr_mask;

	addr = rmi4_data->f01_ctrl_base_addr + 1;

	retval = rmiguest->fn_ptr->write(rmi4_data,
			addr,
			&(rmi4_data->intr_mask[0]),
			sizeof(rmi4_data->intr_mask[0]));
	if (retval < 0)
		return retval;

	return 0;
}

static void rmi_guest_attn(struct synaptics_rmi4_data *rmi4_data,
		unsigned char intr_mask)
{
	if (!rmiguest)
		return;

	if (rmiguest->intr_mask & intr_mask) {
#ifdef USE_GUEST_THREAD
		GMvBraneIsr();
#endif
	}

	return;
}

static int rmi_guest_reinit(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;

	retval = rmi_guest_scan_pdt();
	if (retval < 0)
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to find guest functionality\n",
				__func__);

	return 0;
}

static int rmi_guest_init(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char attr_count;

	rmiguest = kzalloc(sizeof(*rmiguest), GFP_KERNEL);
	if (!rmiguest) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for rmiguest\n",
				__func__);
		retval = -ENOMEM;
		goto err_rmi_guest;
	}

	rmiguest->fn_ptr = kzalloc(sizeof(*(rmiguest->fn_ptr)), GFP_KERNEL);
	if (!rmiguest->fn_ptr) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for fn_ptr\n",
				__func__);
		retval = -ENOMEM;
		goto err_fn_ptr;
	}

	rmiguest->fn_ptr->read = rmi4_data->i2c_read;
	rmiguest->fn_ptr->write = rmi4_data->i2c_write;
	rmiguest->fn_ptr->enable = rmi4_data->irq_enable;
	rmiguest->rmi4_data = rmi4_data;

	retval = rmi_guest_scan_pdt();
	if (retval < 0) {
		retval = 0;
		goto err_scan_pdt;
	}

	rmiguest->sysfs_dir = kobject_create_and_add("guest",
			&rmi4_data->input_dev->dev.kobj);
	if (!rmiguest->sysfs_dir) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to create sysfs directory\n",
				__func__);
		retval = -ENODEV;
		goto err_sysfs_dir;
	}

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = sysfs_create_file(rmiguest->sysfs_dir,
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
	for (attr_count--; attr_count >= 0; attr_count--)
		sysfs_remove_file(rmiguest->sysfs_dir, &attrs[attr_count].attr);

	kobject_put(rmiguest->sysfs_dir);

err_sysfs_dir:
err_scan_pdt:
	kfree(rmiguest->fn_ptr);

err_fn_ptr:
	kfree(rmiguest);
	rmiguest = NULL;

err_rmi_guest:
	return retval;
}

static void rmi_guest_remove(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned char attr_count;

	if (!rmiguest)
		goto exit;

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++)
		sysfs_remove_file(rmiguest->sysfs_dir, &attrs[attr_count].attr);

	kobject_put(rmiguest->sysfs_dir);

	kfree(rmiguest->fn_ptr);
	kfree(rmiguest);
	rmiguest = NULL;

exit:
	return;
}

int rmi_guest_module_register(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;

	retval = synaptics_rmi4_new_function(RMI_GUEST,
			rmi4_data,
			rmi_guest_init,
			rmi_guest_reinit,
			rmi_guest_remove,
			rmi_guest_attn);

	return retval;
}

#ifdef USE_GUEST_THREAD

#define DEB_NONE_LEVEL		0
#define DBG_STRING_LEVEL	1
#define DBG_DATA_LEVEL		2

#define MV_DBG_RESULT_STR_LEN	160
#define MV_DEBUG_STR_LEN	8

#define MV_DEBUG_PRNT_SCREEN(_dest, _temp, _length, fmt, ...)	\
({	\
	snprintf(_temp, _length, fmt, ## __VA_ARGS__);	\
	strcat(_dest, _temp);	\
})

enum MV_ADD_VAL {
	MV_ADDR_HEADER_RX,
	MV_ADDR_HEADER_TX,
	MV_ADDR_PAYLOAD_RX,
	MV_ADDR_PAYLOAD_TX,
	MV_ADDR_ACK_LSB,
	MV_ADDR_ACK_MSB,
	MV_ADDR_VER_LSB,
	MV_ADDR_VER_MSB,
};

enum MV_PKT_ID_TYPE {
	MV_PKT_ID_TYPE_BRANE,
	MV_PKT_ID_TYPE_BASIC,
	MV_PKT_ID_TYPE_START_SVC,
	MV_PKT_ID_TYPE_STOP_SVC,
	MV_PKT_ID_TYPE_DATA_SVC,
	MV_PKT_ID_TYPE_MAX,
};

enum MV_PKT_HEADER {
	MV_HEADER_PKT_SYNC,
	MV_HEADER_PKT_ID,
	MV_HEADER_PKT_ARG,
	MV_HEADER_PKT_MAX,
/*...*/
};

enum MV_PKT_ACK {
	MV_ACK_PKT_ID,
	MV_ACK_PKT_ARG,
	MV_ACK_PKT_MAX,
};

static char * MV_ADDR_STR[8] = {
	"HDR_RX ",
	"HDR_TX ",
	"PLD_RX ",
	"PLD_TX ",
	"ACK_LSB",
	"ACK_MSB",
	"VER_LSB",
	"VER_MSB"
};

static char * MV_PKT_ID_STR[5] = {
	"BRANE",
	"BASIC",
	"START",
	"STOP",
	"DATA",
};

static char * MV_BASIC_PKT_ARG_STR[5] = {
	"WAIT. ",
	"ACK. ",
	"NACK. ",
	"RESET. ",
	"SPEC. ",
};

static void debug_multiverse_pkt(u8 mode, uint16 u16Addr, void *pvData, sint32 s32Size)
{
	char buffer[MV_DBG_RESULT_STR_LEN] = {0,};
	char buffer_temp[MV_DEBUG_STR_LEN] = {0,};
	int i;
	unsigned char addr_id = u16Addr & 0xF;
	unsigned char id_type = MV_PKT_ID_TYPE_MAX;

	if (rmiguest->rmi4_data->guest_pkt_dbg_level >= DBG_DATA_LEVEL) {
		for (i = 0; i < s32Size; i++) {
			MV_DEBUG_PRNT_SCREEN(buffer, buffer_temp, MV_DEBUG_STR_LEN, "0x%02X ", ((unsigned char *)pvData)[i]);
		}
		goto out;
	}

	switch (addr_id) {
	case MV_ADDR_HEADER_RX:
	case MV_ADDR_HEADER_TX:
		for (i = 0; i < MV_HEADER_PKT_MAX; i++) {
			switch (i) {
			case MV_HEADER_PKT_ID:
				id_type	= ((unsigned char *)pvData)[MV_HEADER_PKT_ID];
				MV_DEBUG_PRNT_SCREEN(buffer, buffer_temp, MV_DEBUG_STR_LEN, " %s", MV_PKT_ID_STR[id_type]);
				break;
			case MV_HEADER_PKT_ARG:
				if (id_type == MV_PKT_ID_TYPE_BASIC)
					MV_DEBUG_PRNT_SCREEN(buffer, buffer_temp, MV_DEBUG_STR_LEN, " %s", MV_BASIC_PKT_ARG_STR[((unsigned char *)pvData)[MV_HEADER_PKT_ARG]]);
				else
					MV_DEBUG_PRNT_SCREEN(buffer, buffer_temp, MV_DEBUG_STR_LEN, " [0x%02X]", ((unsigned char *)pvData)[MV_HEADER_PKT_ARG]);
				break;
			default:
				break;
			}
		}
		break;
	case MV_ADDR_ACK_LSB:
		for (i = 0; i < MV_ACK_PKT_MAX; i++) {
			switch (i) {
			case MV_ACK_PKT_ID:
				id_type	= ((unsigned char *)pvData)[MV_HEADER_PKT_ID];
				MV_DEBUG_PRNT_SCREEN(buffer, buffer_temp, MV_DEBUG_STR_LEN, " %s", MV_PKT_ID_STR[((unsigned char *)pvData)[MV_ACK_PKT_ID]]);
				break;
			case MV_ACK_PKT_ARG:
				if (id_type == MV_PKT_ID_TYPE_BASIC)
					MV_DEBUG_PRNT_SCREEN(buffer, buffer_temp, MV_DEBUG_STR_LEN, " %s", MV_BASIC_PKT_ARG_STR[((unsigned char *)pvData)[MV_ACK_PKT_ARG]]);
				else
					MV_DEBUG_PRNT_SCREEN(buffer, buffer_temp, MV_DEBUG_STR_LEN, " [0x%02X]", ((unsigned char *)pvData)[MV_ACK_PKT_ARG]);
				break;
			default:
				break;
			}
		}
		break;
	default:
		for (i = 0; i < s32Size; i++) {
			MV_DEBUG_PRNT_SCREEN(buffer, buffer_temp, MV_DEBUG_STR_LEN, "0x%02X ", ((unsigned char *)pvData)[i]);
		}
		break;
	}

out:
	tsp_debug_err(true, &rmiguest->rmi4_data->i2c_client->dev,
			"[PKT:(%s)]: 0x%04X [%s][%ld]: %s\n", mode ? "W" : "R",
			u16Addr, MV_ADDR_STR[u16Addr & 0xF], s32Size, buffer);
}
#endif

void GMvGtReadI2cRegister(uint16 u16Addr, void *pvData, sint32 s32Size)
{
	if( rmiguest )
	{
		if( rmiguest->fn_ptr && rmiguest->rmi4_data )
		{
			if( rmiguest->fn_ptr->read ) {
				rmiguest->fn_ptr->read( rmiguest->rmi4_data, u16Addr, pvData, s32Size );
#ifdef USE_GUEST_THREAD
				if (rmiguest->rmi4_data->guest_pkt_dbg_level)
					debug_multiverse_pkt(0 ,u16Addr, pvData, s32Size);
#endif
			}
		}
	}
}

void GMvGtWriteI2cRegister(uint16 u16Addr, void *pvData, sint32 s32Size)
{
	if( rmiguest )
	{
		if( rmiguest->fn_ptr && rmiguest->rmi4_data )
		{
			if( rmiguest->fn_ptr->write ) {
				rmiguest->fn_ptr->write( rmiguest->rmi4_data, u16Addr, pvData, s32Size );
#ifdef USE_GUEST_THREAD
				if (rmiguest->rmi4_data->guest_pkt_dbg_level)
					debug_multiverse_pkt(1 ,u16Addr, pvData, s32Size);
#endif
			}
		}
	}
}
