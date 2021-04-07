/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012 Synaptics Incorporated
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
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
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/unaligned.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/firmware.h>
#include "synaptics_i2c_rmi.h"

#define DO_STARTUP_FW_UPDATE
#define STARTUP_FW_UPDATE_DELAY_MS 1000 /* ms */
#define FORCE_UPDATE false
#define DO_LOCKDOWN false

#define MAX_IMAGE_NAME_LEN 256
#define MAX_FIRMWARE_ID_LEN 10

#define F01_DEVICE_STATUS	0X0004

#define BOOTLOADER_ID_OFFSET 0
#define BLOCK_NUMBER_OFFSET 0

#define V5_PROPERTIES_OFFSET 2
#define V5_BLOCK_SIZE_OFFSET 3
#define V5_BLOCK_COUNT_OFFSET 5
#define V5_BLOCK_DATA_OFFSET 2

#define V6_PROPERTIES_OFFSET 1
#define V6_BLOCK_SIZE_OFFSET 2
#define V6_BLOCK_COUNT_OFFSET 3
#define V6_BLOCK_DATA_OFFSET 1
#define V6_FLASH_COMMAND_OFFSET 2
#define V6_FLASH_STATUS_OFFSET 3

#define IMG_VERSION_OFFSET 0x07
#define IMG_X10_TOP_CONTAINER_OFFSET 0x0C
#define IMG_X0_X6_FW_OFFSET 0x100

#define UI_CONFIG_AREA 0x00
#define PERM_CONFIG_AREA 0x01
#define BL_CONFIG_AREA 0x02
#define DISP_CONFIG_AREA 0x03

#define SLEEP_MODE_NORMAL (0x00)
#define SLEEP_MODE_SENSOR_SLEEP (0x01)
#define SLEEP_MODE_RESERVED0 (0x02)
#define SLEEP_MODE_RESERVED1 (0x03)

#define ENABLE_WAIT_MS (1 * 1000)
#define WRITE_WAIT_MS (3 * 1000)
#define ERASE_WAIT_MS (5 * 1000)

#define MIN_SLEEP_TIME_US 50
#define MAX_SLEEP_TIME_US 100
#define STATUS_POLLING_PERIOD_US 3000

#define POLLING_MODE_DEFAULT 0

static ssize_t fwu_sysfs_show_image(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t fwu_sysfs_store_image(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t fwu_sysfs_do_reflash_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t fwu_sysfs_write_config_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t fwu_sysfs_read_config_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t fwu_sysfs_config_area_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

/*
static ssize_t fwu_sysfs_image_name_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
*/
static ssize_t fwu_sysfs_image_size_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t fwu_sysfs_block_size_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t fwu_sysfs_firmware_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t fwu_sysfs_configuration_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t fwu_sysfs_perm_config_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t fwu_sysfs_bl_config_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t fwu_sysfs_disp_config_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t fwu_sysfs_guest_code_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t fwu_sysfs_write_guest_code_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static void fwu_img_parse_format(void);
static int fwu_do_write_guest_code(void);

enum bl_version {
	V5 = 5,
	V6 = 6,
};

enum flash_area {
	NONE,
	UI_FIRMWARE,
	CONFIG_AREA,
};

enum update_mode {
	UPDATE_MODE_NORMAL = 1,
	UPDATE_MODE_FORCE = 2,
	UPDATE_MODE_LOCKDOWN = 8,
};

enum flash_command {
	CMD_IDLE = 0x0,
	CMD_WRITE_FW_BLOCK = 0x2,
	CMD_ERASE_ALL = 0x3,
	CMD_WRITE_LOCKDOWN_BLOCK = 0x4,
	CMD_READ_CONFIG_BLOCK = 0x5,
	CMD_WRITE_CONFIG_BLOCK = 0x6,
	CMD_ERASE_CONFIG = 0x7,
	CMD_READ_SENSOR_ID = 0x8,
	CMD_ERASE_BL_CONFIG = 0x9,
	CMD_ERASE_DISP_CONFIG = 0xA,
	CMD_ERASE_GUEST_CODE = 0xB,
	CMD_WRITE_GUEST_CODE = 0xC,
	CMD_ENABLE_FLASH_PROG = 0xF
};

struct img_x0x6_header {
	/* 0x00 - 0x0f */
	unsigned char checksum[4];
	unsigned char reserved_04;
	unsigned char reserved_05;
	unsigned char options_firmware_id:1;
	unsigned char options_contain_bootloader:1;
	unsigned char options_reserved:6;
	unsigned char bootloader_version;
	unsigned char firmware_size[4];
	unsigned char config_size[4];
	/* 0x10 - 0x1f */
	unsigned char product_id[SYNAPTICS_RMI4_PRODUCT_ID_SIZE];
	unsigned char package_id[2];
	unsigned char package_id_revision[2];
	unsigned char product_info[SYNAPTICS_RMI4_PRODUCT_INFO_SIZE];
	/* 0x20 - 0x2f */
	unsigned char reserved_20_2f[16];
	/* 0x30 - 0x3f */
	unsigned char ds_firmware_info[16];
	/* 0x40 - 0x4f */
	unsigned char ds_info[10];
	unsigned char reserved_4a_4f[6];
	/* 0x50 - 0x53 */
	unsigned char firmware_id[4];
};

enum img_x10_container_id {
	ID_TOP_LEVEL_CONTAINER = 0,
	ID_UI_CONTAINER,
	ID_UI_CONFIGURATION,
	ID_BOOTLOADER_CONTAINER,
	ID_BOOTLOADER_IMAGE_CONTAINER,
	ID_BOOTLOADER_CONFIGURATION_CONTAINER,
	ID_BOOTLOADER_LOCKDOWN_INFORMATION_CONTAINER,
	ID_PERMANENT_CONFIGURATION_CONTAINER,
	ID_GUEST_CODE_CONTAINER,
	ID_BOOTLOADER_PROTOCOL_DESCRIPTOR_CONTAINER,
	ID_UI_PROTOCOL_DESCRIPTOR_CONTAINER,
	ID_RMI_SELF_DISCOVERY_CONTAINER,
	ID_RMI_PAGE_CONTENT_CONTAINER,
	ID_GENERAL_INFORMATION_CONTAINER,
	RESERVERD
};

struct block_data {
	unsigned char *data;
	int size;
};

struct img_file_content {
	unsigned char *fw_image;
	unsigned int image_size;
	unsigned char *image_name;
	unsigned char imageFileVersion;
	struct block_data uiFirmware;
	struct block_data uiConfig;
	struct block_data guestCode;
	struct block_data lockdown;
	struct block_data permanent;
	struct block_data bootloaderInfo;
	unsigned char blMajorVersion;
	unsigned char blMinorVersion;
	unsigned char *configId;	/* len 0x4 */
	unsigned char *firmwareId;	/* len 0x4 */
	unsigned char *packageId;		/* len 0x4 */
	unsigned char *dsFirmwareInfo;	/* len 0x10 */
};

struct img_x10_descriptor {
	unsigned char contentChecksum[4];
	unsigned char containerID[2];
	unsigned char minorVersion;
	unsigned char majorVersion;
	unsigned char reserverd[4];
	unsigned char containerOptionFlags[4];
	unsigned char contentOptionLength[4];
	unsigned char contentOptionAddress[4];
	unsigned char contentLength[4];
	unsigned char contentAddress[4];
};

struct img_x10_bl_container {
	unsigned char majorVersion;
	unsigned char minorVersion;
	unsigned char reserved[2];
	unsigned char *subContainer;
};

struct pdt_properties {
	union {
		struct {
			unsigned char reserved_1:6;
			unsigned char has_bsr:1;
			unsigned char reserved_2:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f01_device_status {
	union {
		struct {
			unsigned char status_code:4;
			unsigned char reserved:2;
			unsigned char flash_prog:1;
			unsigned char unconfigured:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f01_device_control {
	union {
		struct {
			unsigned char sleep_mode:2;
			unsigned char nosleep:1;
			unsigned char reserved:2;
			unsigned char charger_connected:1;
			unsigned char report_rate:1;
			unsigned char configured:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f34_properties_query {
	union {
		struct {
			unsigned char reg_map:1;
			unsigned char unlocked:1;
			unsigned char has_config_id:1;
			unsigned char has_perm_config:1;
			unsigned char has_bl_config:1;
			unsigned char has_disp_config:1;
			unsigned char has_ctrl1:1;
			unsigned char has_flash_query4:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f34_query_04 {
	union {
		struct {
			unsigned char has_guest_code:1;
			unsigned char reserved:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_fwu_handle {
	enum bl_version bl_version;
	bool initialized;
	bool program_enabled;
	bool has_perm_config;
	bool has_bl_config;
	bool has_disp_config;
	bool has_guest_code;
	bool force_update;
	bool in_flash_prog_mode;
	bool do_lockdown;
	bool can_guest_bootloader;
	unsigned int data_pos;
	unsigned char *ext_data_source;
	unsigned char *read_config_buf;
	unsigned char intr_mask;
	unsigned char command;
	unsigned char bootloader_id[4];
	unsigned char flash_status;
	unsigned char productinfo1;
	unsigned char productinfo2;
	unsigned char properties_off;
	unsigned char blk_size_off;
	unsigned char blk_count_off;
	unsigned char blk_data_off;
	unsigned char properties2_off;
	unsigned char guest_blk_count_off;
	unsigned char flash_cmd_off;
	unsigned char flash_status_off;
	unsigned short block_size;
	unsigned short fw_block_count;
	unsigned short config_block_count;
	unsigned short perm_config_block_count;
	unsigned short bl_config_block_count;
	unsigned short disp_config_block_count;
	unsigned short guest_code_block_count;
	unsigned short config_size;
	unsigned short config_area;
	char product_id[SYNAPTICS_RMI4_PRODUCT_ID_SIZE + 1];

	struct f34_properties_query flash_properties;
	struct workqueue_struct *fwu_workqueue;
	struct delayed_work fwu_work;
	struct synaptics_rmi4_fn_desc f01_fd;
	struct synaptics_rmi4_fn_desc f34_fd;
	struct synaptics_rmi4_exp_fn_ptr *fn_ptr;
	struct synaptics_rmi4_data *rmi4_data;
	struct img_file_content img;

	bool polling_mode;
};

static struct bin_attribute dev_attr_data = {
	.attr = {
		.name = "data",
		.mode = (S_IRUGO | S_IWUSR | S_IWGRP),
	},
	.size = 0,
	.read = fwu_sysfs_show_image,
	.write = fwu_sysfs_store_image,
};

static struct device_attribute attrs[] = {
	__ATTR(doreflash, S_IWUSR | S_IWGRP,
			synaptics_rmi4_show_error,
			fwu_sysfs_do_reflash_store),
	__ATTR(writeconfig, S_IWUSR | S_IWGRP,
			synaptics_rmi4_show_error,
			fwu_sysfs_write_config_store),
	__ATTR(readconfig, S_IWUSR | S_IWGRP,
			synaptics_rmi4_show_error,
			fwu_sysfs_read_config_store),
	__ATTR(configarea, S_IWUSR | S_IWGRP,
			synaptics_rmi4_show_error,
			fwu_sysfs_config_area_store),
	__ATTR(imagesize, S_IWUSR | S_IWGRP,
			synaptics_rmi4_show_error,
			fwu_sysfs_image_size_store),
	__ATTR(blocksize, S_IRUGO,
			fwu_sysfs_block_size_show,
			synaptics_rmi4_store_error),
	__ATTR(fwblockcount, S_IRUGO,
			fwu_sysfs_firmware_block_count_show,
			synaptics_rmi4_store_error),
	__ATTR(configblockcount, S_IRUGO,
			fwu_sysfs_configuration_block_count_show,
			synaptics_rmi4_store_error),
	__ATTR(permconfigblockcount, S_IRUGO,
			fwu_sysfs_perm_config_block_count_show,
			synaptics_rmi4_store_error),
	__ATTR(blconfigblockcount, S_IRUGO,
			fwu_sysfs_bl_config_block_count_show,
			synaptics_rmi4_store_error),
	__ATTR(dispconfigblockcount, S_IRUGO,
			fwu_sysfs_disp_config_block_count_show,
			synaptics_rmi4_store_error),
	__ATTR(guestcodeblockcount, S_IRUGO,
			fwu_sysfs_guest_code_block_count_show,
			synaptics_rmi4_store_error),
	__ATTR(writeguestcode, S_IWUSR | S_IWGRP,
			synaptics_rmi4_show_error,
			fwu_sysfs_write_guest_code_store),
};

static struct synaptics_rmi4_fwu_handle *fwu;

static unsigned int extract_uint_le(const unsigned char *ptr)
{
	return (unsigned int)ptr[0] +
		(unsigned int)ptr[1] * 0x100 +
		(unsigned int)ptr[2] * 0x10000 +
		(unsigned int)ptr[3] * 0x1000000;
}

#ifdef FW_UPDATE_GO_NOGO
static unsigned int extract_uint_be(const unsigned char *ptr)
{
	return (unsigned int)ptr[3] +
			(unsigned int)ptr[2] * 0x100 +
			(unsigned int)ptr[1] * 0x10000 +
			(unsigned int)ptr[0] * 0x1000000;
}
#endif

static unsigned short extract_ushort_le(const unsigned char *ptr)
{
	return (unsigned int)ptr[0] + (unsigned int)ptr[1] * 0x100;
}

static int fwu_read_f01_device_status(struct f01_device_status *status)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	retval = fwu->fn_ptr->read(rmi4_data,
			fwu->f01_fd.data_base_addr,
			status->data,
			sizeof(status->data));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read F01 device status\n",
				__func__);
		return retval;
	}

	return 0;
}

static int fwu_read_f34_queries(void)
{
	int retval;
	unsigned char count;
	unsigned char buf[10];
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	retval = fwu->fn_ptr->read(rmi4_data,
			fwu->f34_fd.query_base_addr + BOOTLOADER_ID_OFFSET,
			fwu->bootloader_id,
			sizeof(fwu->bootloader_id));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read bootloader ID\n",
				__func__);
		return retval;
	}

	if (fwu->bootloader_id[1] == '5') {
		fwu->bl_version = V5;
	} else if (fwu->bootloader_id[1] == '6') {
		fwu->bl_version = V6;
	} else {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Unrecognized bootloader version\n",
				__func__);
		return -EINVAL;
	}

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: F34 Query : ID: %s\n", __func__, fwu->bootloader_id);

	if (fwu->bl_version == V5) {
		fwu->properties_off = V5_PROPERTIES_OFFSET;
		fwu->blk_size_off = V5_BLOCK_SIZE_OFFSET;
		fwu->blk_count_off = V5_BLOCK_COUNT_OFFSET;
		fwu->blk_data_off = V5_BLOCK_DATA_OFFSET;
	} else if (fwu->bl_version == V6) {
		fwu->properties_off = V6_PROPERTIES_OFFSET;
		fwu->blk_size_off = V6_BLOCK_SIZE_OFFSET;
		fwu->blk_count_off = V6_BLOCK_COUNT_OFFSET;
		fwu->blk_data_off = V6_BLOCK_DATA_OFFSET;
		fwu->properties2_off = fwu->blk_count_off + 1;
		fwu->guest_blk_count_off = fwu->properties2_off + 1;
	}

	retval = fwu->fn_ptr->read(rmi4_data,
			fwu->f34_fd.query_base_addr + fwu->properties_off,
			fwu->flash_properties.data,
			sizeof(fwu->flash_properties.data));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read flash properties\n",
				__func__);
		return retval;
	}

	count = 4;

	if (fwu->flash_properties.has_perm_config) {
		fwu->has_perm_config = 1;
		count += 2;
	}

	if (fwu->flash_properties.has_bl_config) {
		fwu->has_bl_config = 1;
		count += 2;
	}

	if (fwu->flash_properties.has_disp_config) {
		fwu->has_disp_config = 1;
		count += 2;
	}

	if (fwu->flash_properties.has_flash_query4) {
		struct f34_query_04 query4;

		retval = fwu->fn_ptr->read(rmi4_data,
			fwu->f34_fd.query_base_addr + fwu->properties2_off,
			query4.data,
			sizeof(query4.data));
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to read block size info\n",
					__func__);
			return retval;
		}

		if (query4.has_guest_code) {
			retval = fwu->fn_ptr->read(rmi4_data,
				fwu->f34_fd.query_base_addr + fwu->guest_blk_count_off,
				buf,
				2);
			if (retval < 0) {
				tsp_debug_err(true, &rmi4_data->i2c_client->dev,
						"%s: Failed to read block size info\n",
						__func__);
				return retval;
			}
			batohs(&fwu->guest_code_block_count, buf);
			fwu->has_guest_code = 1;
		} else {
			tsp_debug_info(true, &rmi4_data->i2c_client->dev,
					"%s: query data do not supply quest image.\n",
					__func__);

		}
	}

	retval = fwu->fn_ptr->read(rmi4_data,
			fwu->f34_fd.query_base_addr + fwu->blk_size_off,
			buf,
			2);
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read block size info\n",
				__func__);
		return retval;
	}

	batohs(&fwu->block_size, &(buf[0]));

	if (fwu->bl_version == V5) {
		fwu->flash_cmd_off = fwu->blk_data_off + fwu->block_size;
		fwu->flash_status_off = fwu->flash_cmd_off;
	} else if (fwu->bl_version == V6) {
		fwu->flash_cmd_off = V6_FLASH_COMMAND_OFFSET;
		fwu->flash_status_off = V6_FLASH_STATUS_OFFSET;
	}

	retval = fwu->fn_ptr->read(fwu->rmi4_data,
			fwu->f34_fd.query_base_addr + fwu->blk_count_off,
			buf,
			count);
	if (retval < 0) {
		tsp_debug_err(true, &fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to read block count info\n",
				__func__);
		return retval;
	}

	batohs(&fwu->fw_block_count, &(buf[0]));
	batohs(&fwu->config_block_count, &(buf[2]));

	count = 4;

	if (fwu->has_perm_config) {
		batohs(&fwu->perm_config_block_count, &(buf[count]));
		count += 2;
	}

	if (fwu->has_bl_config) {
		batohs(&fwu->bl_config_block_count, &(buf[count]));
		count += 2;
	}

	if (fwu->has_disp_config)
		batohs(&fwu->disp_config_block_count, &(buf[count]));

	return 0;
}

static int fwu_read_f34_flash_status(void)
{
	int retval;
	unsigned char status;
	unsigned char command;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	retval = fwu->fn_ptr->read(rmi4_data,
			fwu->f34_fd.data_base_addr + fwu->flash_status_off,
			&status,
			sizeof(status));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read flash status\n",
				__func__);
		return retval;
	}

	fwu->program_enabled = status >> 7;

	if (fwu->bl_version == V5)
		fwu->flash_status = (status >> 4) & MASK_3BIT;
	else if (fwu->bl_version == V6)
		fwu->flash_status = status & MASK_3BIT;

	retval = fwu->fn_ptr->read(rmi4_data,
			fwu->f34_fd.data_base_addr + fwu->flash_cmd_off,
			&command,
			sizeof(command));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read flash command\n",
				__func__);
		return retval;
	}

	fwu->command = command & MASK_4BIT;

	return 0;
}

static int fwu_write_f34_command(unsigned char cmd)
{
	int retval;
	unsigned char command = cmd & MASK_4BIT;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	fwu->command = cmd;

	retval = fwu->fn_ptr->write(rmi4_data,
			fwu->f34_fd.data_base_addr + fwu->flash_cmd_off,
			&command,
			sizeof(command));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write command 0x%02x\n",
				__func__, command);
		return retval;
	}

	return 0;
}

static int fwu_wait_for_idle(int timeout_ms)
{
	int count = 0;
	int timeout_count = ((timeout_ms * 1000) / MAX_SLEEP_TIME_US) + 1;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	do {
		if (fwu->polling_mode || count == timeout_count)
			fwu_read_f34_flash_status();
		if ((fwu->command == 0x00) && (fwu->flash_status == 0x00)) {
			if (count == timeout_count)
				fwu->polling_mode = true;
			return 0;
		}
		usleep_range(MIN_SLEEP_TIME_US, MAX_SLEEP_TIME_US);
		count++;

	} while (count <= timeout_count);

	tsp_debug_err(true, &rmi4_data->i2c_client->dev,
			"%s: Timed out waiting for idle status\n",
			__func__);

	return -ETIMEDOUT;
}

#ifdef FW_UPDATE_GO_NOGO
static enum flash_area fwu_go_nogo(void)
{
	int retval;
	enum flash_area flash_area = NONE;
	unsigned char index = 0;
	unsigned char config_id[4];
	unsigned int device_config_id;
	unsigned int image_config_id;
	unsigned int device_fw_id;
	unsigned long image_fw_id;
	char *strptr;
	char *firmware_id;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (fwu->force_update) {
		flash_area = UI_FIRMWARE;
		goto exit;
	}

	/* Update both UI and config if device is in bootloader mode */
	if (fwu->in_flash_prog_mode) {
		flash_area = UI_FIRMWARE;
		goto exit;
	}

	/* Get device firmware ID */
	device_fw_id = rmi4_data->rmi4_mod_info.build_id[0] +
			rmi4_data->rmi4_mod_info.build_id[1] * 0x100 +
			rmi4_data->rmi4_mod_info.build_id[2] * 0x10000;
	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Device firmware ID = %d\n",
			__func__, device_fw_id);

	/* Get image firmware ID */
	if (fwu->img.firmwareId != NULL) {
		image_fw_id = extract_uint_le(fwu->img.firmwareId);
	} else {
		strptr = strstr(fwu->img.image_name, "PR");
		if (!strptr) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: No valid PR number (PRxxxxxxx) "
					"found in image file name (%s)\n",
					__func__, fwu->img.image_name);
			flash_area = NONE;
			goto exit;
		}

		strptr += 2;
		firmware_id = kzalloc(MAX_FIRMWARE_ID_LEN, GFP_KERNEL);
		while (strptr[index] >= '0' && strptr[index] <= '9') {
			firmware_id[index] = strptr[index];
			index++;
		}

		retval = kstrtoul(firmware_id, 10, &image_fw_id);
		kfree(firmware_id);
		if (retval) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to obtain image firmware ID\n",
					__func__);
			flash_area = NONE;
			goto exit;
		}
	}
	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Image firmware ID = %d\n",
			__func__, (unsigned int)image_fw_id);

	if (image_fw_id > device_fw_id) {
		flash_area = UI_FIRMWARE;
		goto exit;
	} else if (image_fw_id < device_fw_id) {
		tsp_debug_info(true, &rmi4_data->i2c_client->dev,
				"%s: Image firmware ID older than device firmware ID\n",
				__func__);
		flash_area = NONE;
		goto exit;
	}

	/* Get device config ID */
	retval = fwu->fn_ptr->read(rmi4_data,
				fwu->f34_fd.ctrl_base_addr,
				config_id,
				sizeof(config_id));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read device config ID\n",
				__func__);
		flash_area = NONE;
		goto exit;
	}
	device_config_id = extract_uint_be(config_id);
	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Device config ID = 0x%02x 0x%02x 0x%02x 0x%02x\n",
			__func__,
			config_id[0],
			config_id[1],
			config_id[2],
			config_id[3]);

	/* Get image config ID */
	image_config_id = extract_uint_be(fwu->img.configId);
	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Image config ID = 0x%02x 0x%02x 0x%02x 0x%02x\n",
			__func__,
			fwu->img.configId[0],
			fwu->img.configId[1],
			fwu->img.configId[2],
			fwu->img.configId[3]);

	if (image_config_id > device_config_id) {
		flash_area = CONFIG_AREA;
		goto exit;
	}

exit:
	if (flash_area == NONE) {
		tsp_debug_info(true, &rmi4_data->i2c_client->dev,
				"%s: No need to do reflash\n",
				__func__);
	} else {
		tsp_debug_info(true, &rmi4_data->i2c_client->dev,
				"%s: Updating %s\n",
				__func__,
				flash_area == UI_FIRMWARE ?
				"UI firmware" :
				"config only");
	}
	return flash_area;
}
#endif

static int fwu_scan_pdt(void)
{
	int retval;
	unsigned char ii;
	unsigned char intr_count = 0;
	unsigned char intr_off;
	unsigned char intr_src;
	unsigned short addr;
	bool f01found = false;
	bool f34found = false;
	struct synaptics_rmi4_fn_desc rmi_fd;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	for (addr = PDT_START; addr > PDT_END; addr -= PDT_ENTRY_SIZE) {
		retval = fwu->fn_ptr->read(rmi4_data,
				addr,
				(unsigned char *)&rmi_fd,
				sizeof(rmi_fd));
		if (retval < 0)
			return retval;

		if (rmi_fd.fn_number) {
			tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
					"%s: Found F%02x\n",
					__func__, rmi_fd.fn_number);
			switch (rmi_fd.fn_number) {
			case SYNAPTICS_RMI4_F01:
				f01found = true;
				fwu->f01_fd.query_base_addr =
					rmi_fd.query_base_addr;
				fwu->f01_fd.ctrl_base_addr =
					rmi_fd.ctrl_base_addr;
				fwu->f01_fd.data_base_addr =
					rmi_fd.data_base_addr;
				fwu->f01_fd.cmd_base_addr =
					rmi_fd.cmd_base_addr;
				break;
			case SYNAPTICS_RMI4_F34:
				f34found = true;
				fwu->f34_fd.query_base_addr =
					rmi_fd.query_base_addr;
				fwu->f34_fd.ctrl_base_addr =
					rmi_fd.ctrl_base_addr;
				fwu->f34_fd.data_base_addr =
					rmi_fd.data_base_addr;
				fwu->intr_mask = 0;
				intr_src = rmi_fd.intr_src_count;
				intr_off = intr_count % 8;
				for (ii = intr_off;	ii < ((intr_src & MASK_3BIT) + intr_off); ii++)
					fwu->intr_mask |= 1 << ii;
				break;
			}
		} else {
			break;
		}

		intr_count += (rmi_fd.intr_src_count & MASK_3BIT);
	}

	if (!f01found || !f34found) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to find both F01 and F34\n",
				__func__);
		return -EINVAL;
	}

	return 0;
}

static int fwu_write_blocks(unsigned char *block_ptr, unsigned int block_size,
		enum flash_command command)
{
	int retval;
	unsigned char block_offset[] = {0, 0};
	unsigned short block_num;
	unsigned short block_cnt = block_size / fwu->block_size;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (block_ptr == NULL) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Cannot find block data (%x)\n",
				__func__, command);
		return -EINVAL;
	}

	if (command == CMD_WRITE_CONFIG_BLOCK)
		block_offset[1] |= (fwu->config_area << 5);

	retval = fwu->fn_ptr->write(rmi4_data,
			fwu->f34_fd.data_base_addr + BLOCK_NUMBER_OFFSET,
			block_offset,
			sizeof(block_offset));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write to block number registers\n",
				__func__);
		return retval;
	}

	for (block_num = 0; block_num < block_cnt; block_num++) {
		retval = fwu->fn_ptr->write(rmi4_data,
				fwu->f34_fd.data_base_addr + fwu->blk_data_off,
				block_ptr,
				fwu->block_size);
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to write block data (block %d)\n",
					__func__, block_num);
			return retval;
		}

		retval = fwu_write_f34_command(command);
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to write command for block %d\n",
					__func__, block_num);
			return retval;
		}

		retval = fwu_wait_for_idle(WRITE_WAIT_MS);
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to wait for idle status (block %d)\n",
					__func__, block_num);
			return retval;
		}

		block_ptr += fwu->block_size;
	}

	return 0;
}

static int fwu_write_firmware_block(void)
{
	tsp_debug_info(true, &fwu->rmi4_data->i2c_client->dev,
			"%s: size : %d, cmd : %d\n", __func__, fwu->img.uiFirmware.size, CMD_WRITE_FW_BLOCK);

	return fwu_write_blocks(fwu->img.uiFirmware.data,
		fwu->img.uiFirmware.size, CMD_WRITE_FW_BLOCK);
}

static int fwu_write_config_block(void)
{
	tsp_debug_info(true, &fwu->rmi4_data->i2c_client->dev,
			"%s: size : %d, cmd : %d\n", __func__, fwu->img.uiConfig.size, CMD_WRITE_CONFIG_BLOCK);

	return fwu_write_blocks(fwu->img.uiConfig.data,
		fwu->img.uiConfig.size, CMD_WRITE_CONFIG_BLOCK);
}

/*
static int fwu_write_lockdown_block(void)
{
	return fwu_write_blocks(fwu->img.lockdown.data,
		fwu->img.lockdown.size, CMD_WRITE_LOCKDOWN_BLOCK);
}
*/
static int fwu_write_guest_code_block(void)
{
	return fwu_write_blocks(fwu->img.guestCode.data,
		fwu->img.guestCode.size, CMD_WRITE_GUEST_CODE);
}

static int fwu_write_bootloader_id(void)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	retval = fwu->fn_ptr->write(rmi4_data,
			fwu->f34_fd.data_base_addr + fwu->blk_data_off,
			fwu->bootloader_id,
			sizeof(fwu->bootloader_id));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write bootloader ID\n",
				__func__);
		return retval;
	}

	return 0;
}

static int fwu_enter_flash_prog(void)
{
	int retval;
	struct f01_device_status f01_device_status;
	struct f01_device_control f01_device_control;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;
	retval = fwu_read_f34_flash_status();
	if (retval < 0)
		return retval;

	if (fwu->program_enabled)
		return 0;

	retval = fwu_write_bootloader_id();
	if (retval < 0)
		return retval;

	retval = fwu_write_f34_command(CMD_ENABLE_FLASH_PROG);
	if (retval < 0)
		return retval;

	retval = fwu_wait_for_idle(ENABLE_WAIT_MS);
	if (retval < 0)
		return retval;

	if (!fwu->program_enabled) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Program enabled bit not set\n",
				__func__);
		return -EINVAL;
	}

	retval = fwu_scan_pdt();
	if (retval < 0)
		return retval;

	retval = fwu_read_f01_device_status(&f01_device_status);
	if (retval < 0)
		return retval;

	if (!f01_device_status.flash_prog) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Not in flash prog mode\n",
				__func__);
		return -EINVAL;
	}

	retval = fwu_read_f34_queries();
	if (retval < 0)
		return retval;

	retval = fwu->fn_ptr->read(rmi4_data,
			fwu->f01_fd.ctrl_base_addr,
			f01_device_control.data,
			sizeof(f01_device_control.data));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read F01 device control\n",
				__func__);
		return retval;
	}

	f01_device_control.nosleep = true;
	f01_device_control.sleep_mode = SLEEP_MODE_NORMAL;

	retval = fwu->fn_ptr->write(rmi4_data,
			fwu->f01_fd.ctrl_base_addr,
			f01_device_control.data,
			sizeof(f01_device_control.data));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write F01 device control\n",
				__func__);
		return retval;
	}

	return retval;
}

static int fwu_do_reflash(void)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;
	retval = fwu_enter_flash_prog();
	if (retval < 0)
		return retval;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Entered flash prog mode\n",
			__func__);

	retval = fwu_write_bootloader_id();
	if (retval < 0)
		return retval;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Bootloader ID written\n",
			__func__);

	retval = fwu_write_f34_command(CMD_ERASE_ALL);
	if (retval < 0)
		return retval;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Erase all command written\n",
			__func__);
	usleep_range(MAX_SLEEP_TIME_US, MAX_SLEEP_TIME_US + MIN_SLEEP_TIME_US);

	retval = fwu_wait_for_idle(ERASE_WAIT_MS);
	if (retval < 0)
		return retval;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Idle status detected\n",
			__func__);

	if (fwu->img.uiFirmware.data) {
		retval = fwu_write_firmware_block();
		if (retval < 0)
			return retval;
		tsp_debug_info(true, &rmi4_data->i2c_client->dev, "%s: Firmware programmed\n",
			__func__);
	}

	if (fwu->img.uiConfig.data) {
		retval = fwu_write_config_block();
		if (retval < 0)
			return retval;
		tsp_debug_info(true, &rmi4_data->i2c_client->dev, "%s: Configuration programmed\n",
			__func__);
	}

	if (fwu->img.guestCode.data) {
		retval = fwu_do_write_guest_code();
		if (retval < 0)
			return retval;
		tsp_debug_info(true, &rmi4_data->i2c_client->dev, "%s: guest-thread programmed\n",
				__func__);
	} else {
		tsp_debug_info(true, &rmi4_data->i2c_client->dev, "%s: is not guestcode data in fw img\n",
				__func__);

	}
	return retval;
}

static int fwu_do_write_config(void)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	retval = fwu_enter_flash_prog();
	if (retval < 0)
		return retval;

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: Entered flash prog mode\n",
			__func__);

	if (fwu->config_area == PERM_CONFIG_AREA) {
		fwu->config_block_count = fwu->perm_config_block_count;
		goto write_config;
	}

	retval = fwu_write_bootloader_id();
	if (retval < 0)
		return retval;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Bootloader ID written, AREA : %d\n",
			__func__, fwu->config_area);

	switch (fwu->config_area) {
	case UI_CONFIG_AREA:
		retval = fwu_write_f34_command(CMD_ERASE_CONFIG);
		break;
	case BL_CONFIG_AREA:
		retval = fwu_write_f34_command(CMD_ERASE_BL_CONFIG);
		fwu->config_block_count = fwu->bl_config_block_count;
		break;
	case DISP_CONFIG_AREA:
		retval = fwu_write_f34_command(CMD_ERASE_DISP_CONFIG);
		fwu->config_block_count = fwu->disp_config_block_count;
		break;
	}
	if (retval < 0)
		return retval;

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: Erase command written\n",
			__func__);

	retval = fwu_wait_for_idle(ERASE_WAIT_MS);
	if (retval < 0)
		return retval;

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: Idle status detected\n",
			__func__);

write_config:
	retval = fwu_write_config_block();
	if (retval < 0)
		return retval;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev, "%s: Config written\n",
		__func__);

	return retval;
}

static int fwu_start_reflash(void)
{
	int retval = 0, retry = 3;
#ifdef FW_UPDATE_GO_NOGO
	enum flash_area flash_area;
#endif
	struct f01_device_status f01_device_status;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;


	if (rmi4_data->sensor_sleep || rmi4_data->touch_stopped) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Sensor sleeping or stopped\n",
				__func__);
		return -ENODEV;
	}

	rmi4_data->stay_awake = true;
	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: start\n", __func__);

	if (!fwu->ext_data_source) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Firmware data is NULL\n", __func__);
		return -ENODEV;
	}

	fwu->img.fw_image = fwu->ext_data_source;
	fwu_img_parse_format();

	if (fwu->bl_version != fwu->img.blMajorVersion) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Bootloader version mismatch\n",
				__func__);
		retval = -EINVAL;
		goto exit;
	}

	while (retry--) {
		mutex_lock(&(rmi4_data->rmi4_reflash_mutex));
		rmi4_data->doing_reflash = true;

#ifdef FW_UPDATE_GO_NOGO
		flash_area = fwu_go_nogo();

		switch (flash_area) {
		case UI_FIRMWARE:
			retval = fwu_do_reflash();
			break;
		case CONFIG_AREA:
			retval = fwu_do_write_config();
			break;
		case NONE:
		default:
			tsp_debug_err(true, &rmi4_data->i2c_client->dev, "%s: case is NONE or default\n",
					__func__);
			goto exit;
		}
#else
		retval = fwu_do_reflash();
#endif
		if (retval < 0)
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to do reflash\n", __func__);
exit:
		rmi4_data->reset_device(rmi4_data);
		rmi4_data->doing_reflash = false;
		mutex_unlock(&(rmi4_data->rmi4_reflash_mutex));

		retval = fwu_read_f01_device_status(&f01_device_status);
		if (retval < 0)
			goto exit;
		else
			break;
	}

	tsp_debug_info(true, &rmi4_data->i2c_client->dev, "%s: End of reflash process\n",
		 __func__);

	rmi4_data->stay_awake = false;

	return retval;
}

static int fwu_do_write_guest_code(void)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (!fwu->has_guest_code) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
			"%s: Firmware does not support Guest Code.\n",
			__func__);
		retval = -EINVAL;
	}
	if (fwu->guest_code_block_count !=
		(fwu->img.guestCode.size/fwu->block_size)) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
			"%s: Size of Guest Code not match (dev: %x, img: %x).\n",
			__func__, fwu->guest_code_block_count,
			fwu->img.guestCode.size/fwu->block_size);
		retval = -EINVAL;
	}

	retval = fwu_enter_flash_prog();
	if (retval < 0)
		return retval;

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: Entered flash prog mode\n",
			__func__);

	retval = fwu_write_bootloader_id();
	if (retval < 0)
		return retval;

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: Bootloader ID written\n",
			__func__);

	retval = fwu_write_f34_command(CMD_ERASE_GUEST_CODE);
	if (retval < 0)
		return retval;

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: Erase command written\n",
			__func__);

	retval = fwu_wait_for_idle(ERASE_WAIT_MS);
	if (retval < 0)
		return retval;

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: Idle status detected\n",
			__func__);

	retval = fwu_write_guest_code_block();
	if (retval < 0)
		return retval;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev, "%s: guest code written\n",
			__func__);

	return retval;
}

static int fwu_start_write_config(void)
{
	int retval;
	unsigned short block_count;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	switch (fwu->config_area) {
	case UI_CONFIG_AREA:
		block_count = fwu->config_block_count;
		break;
	case PERM_CONFIG_AREA:
		if (!fwu->has_perm_config)
			return -EINVAL;
		block_count = fwu->perm_config_block_count;
		break;
	case BL_CONFIG_AREA:
		if (!fwu->has_bl_config)
			return -EINVAL;
		block_count = fwu->bl_config_block_count;
		break;
	case DISP_CONFIG_AREA:
		if (!fwu->has_disp_config)
			return -EINVAL;
		block_count = fwu->disp_config_block_count;
		break;
	default:
		return -EINVAL;
	}

	if (fwu->ext_data_source)
		fwu->img.uiConfig.data = fwu->ext_data_source;
	else
		return -EINVAL;

	fwu->config_size = fwu->block_size * block_count;

	/* Jump to the config area if given a packrat image */
	if ((fwu->config_area == UI_CONFIG_AREA) &&
			(fwu->config_size != fwu->img.image_size)) {
		fwu_img_parse_format();
	}

	tsp_debug_info(true, &rmi4_data->i2c_client->dev, "%s: Start of write config process\n",
		__func__);

	rmi4_data->doing_reflash = true;
	retval = fwu_do_write_config();
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write config\n",
				__func__);
	}

	rmi4_data->reset_device(rmi4_data);
	rmi4_data->doing_reflash = false;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev, "%s: End of write config process\n",
		__func__);

	return retval;
}

#define CHECKSUM_SIZE	4
static void synaptics_rmi_calculate_checksum(unsigned short *data,
				unsigned short len, unsigned long *result)
{
	unsigned long temp;
	unsigned long sum1 = 0xffff;
	unsigned long sum2 = 0xffff;

	*result = 0xffffffff;

	while (len--) {
		temp = *data;
		sum1 += temp;
		sum2 += sum1;
		sum1 = (sum1 & 0xffff) + (sum1 >> 16);
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);
		data++;
	}

	*result = sum2 << 16 | sum1;

	return;
}

static void synaptics_rmi_rewrite_checksum(unsigned char *dest,
				unsigned long src)
{
	dest[0] = (unsigned char)(src & 0xff);
	dest[1] = (unsigned char)((src >> 8) & 0xff);
	dest[2] = (unsigned char)((src >> 16) & 0xff);
	dest[3] = (unsigned char)((src >> 24) & 0xff);

	return;
}

int synaptics_rmi4_set_tsp_test_result_in_config(int value)

{
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;
	int retval;
	unsigned char buf[10] = {0, };
	unsigned long checksum;

	/* read config from IC */
	memset(buf, 0, sizeof(buf));
	snprintf(buf, 2, "%u\n", 1);
	fwu_sysfs_read_config_store(&rmi4_data->i2c_client->dev, NULL, buf, 1);

	/* set test result value
	 * MSB 4bit of Customr derined config ID 0 used for factory test in TSP.
	 * PASS : 2, FAIL : 1, NONE: 0.
	 */
	fwu->read_config_buf[0] &= 0x0F;
	fwu->read_config_buf[0] |= value << 4;

	/* check CRC checksum value and re-write checksum in config */
	synaptics_rmi_calculate_checksum((unsigned short *)fwu->read_config_buf,
			(fwu->config_size - CHECKSUM_SIZE) / 2, &checksum);

	synaptics_rmi_rewrite_checksum(&fwu->read_config_buf[fwu->config_size - CHECKSUM_SIZE],
			checksum);

	rmi4_data->doing_reflash = true;

	retval = fwu_enter_flash_prog();
	if (retval < 0)
		goto err_config_write;


	retval = fwu_write_bootloader_id();
	if (retval < 0)
		goto err_config_write;

	tsp_debug_info(true, &fwu->rmi4_data->i2c_client->dev,
			"%s: Bootloader ID written\n",
			__func__);

	retval = fwu_write_f34_command(CMD_ERASE_CONFIG);
	if (retval < 0)
		goto err_config_write;

	tsp_debug_info(true, &fwu->rmi4_data->i2c_client->dev,
			"%s: Erase command written\n",
			__func__);

	retval = fwu_wait_for_idle(ERASE_WAIT_MS);
	if (retval < 0)
		goto err_config_write;

	tsp_debug_info(true, &fwu->rmi4_data->i2c_client->dev,
			"%s: Idle status detected\n",
			__func__);

	fwu_write_blocks(fwu->read_config_buf,
			fwu->config_size, CMD_WRITE_CONFIG_BLOCK);

	tsp_debug_info(true, &fwu->rmi4_data->i2c_client->dev, "%s: Config written\n",
			__func__);

err_config_write:
	fwu->rmi4_data->reset_device(fwu->rmi4_data);
	fwu->rmi4_data->doing_reflash = false;

	return retval;
}

static int fwu_start_write_guest_code(void)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (!fwu->ext_data_source)
		return -EINVAL;

	fwu_img_parse_format();

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Start of update guest code process\n", __func__);

	retval = fwu_do_write_guest_code();
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write config\n",
				__func__);
	}

	rmi4_data->reset_device(rmi4_data);

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: End of write guest code process\n", __func__);

	return retval;
}

int fwu_do_read_config(void)
{
	int retval;
	unsigned char block_offset[] = {0, 0};
	unsigned short block_num;
	unsigned short block_count;
	unsigned short index = 0;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	retval = fwu_enter_flash_prog();
	if (retval < 0)
		goto exit;

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: Entered flash prog mode\n",
			__func__);

	switch (fwu->config_area) {
	case UI_CONFIG_AREA:
		block_count = fwu->config_block_count;
		break;
	case PERM_CONFIG_AREA:
		if (!fwu->has_perm_config) {
			retval = -EINVAL;
			goto exit;
		}
		block_count = fwu->perm_config_block_count;
		break;
	case BL_CONFIG_AREA:
		if (!fwu->has_bl_config) {
			retval = -EINVAL;
			goto exit;
		}
		block_count = fwu->bl_config_block_count;
		break;
	case DISP_CONFIG_AREA:
		if (!fwu->has_disp_config) {
			retval = -EINVAL;
			goto exit;
		}
		block_count = fwu->disp_config_block_count;
		break;
	default:
		retval = -EINVAL;
		goto exit;
	}

	fwu->config_size = fwu->block_size * block_count;

	kfree(fwu->read_config_buf);
	fwu->read_config_buf = kzalloc(fwu->config_size, GFP_KERNEL);
	if (!fwu->read_config_buf) {
		tsp_debug_err(true, &fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for config size data\n",
				__func__);
		return -ENOMEM;
	}

	block_offset[1] |= (fwu->config_area << 5);

	retval = fwu->fn_ptr->write(fwu->rmi4_data,
			fwu->f34_fd.data_base_addr + BLOCK_NUMBER_OFFSET,
			block_offset,
			sizeof(block_offset));
	if (retval < 0) {
		tsp_debug_err(true, &fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to write to block number registers\n",
				__func__);
		goto exit;
	}

	for (block_num = 0; block_num < block_count; block_num++) {
		retval = fwu_write_f34_command(CMD_READ_CONFIG_BLOCK);
		if (retval < 0) {
			tsp_debug_err(true, &fwu->rmi4_data->i2c_client->dev,
					"%s: Failed to write read config command\n",
					__func__);
			goto exit;
		}

		retval = fwu_wait_for_idle(WRITE_WAIT_MS);
		if (retval < 0) {
			tsp_debug_err(true, &fwu->rmi4_data->i2c_client->dev,
					"%s: Failed to wait for idle status\n",
					__func__);
			goto exit;
		}

		retval = fwu->fn_ptr->read(fwu->rmi4_data,
				fwu->f34_fd.data_base_addr + fwu->blk_data_off,
				&fwu->read_config_buf[index],
				fwu->block_size);
		if (retval < 0) {
			tsp_debug_err(true, &fwu->rmi4_data->i2c_client->dev,
					"%s: Failed to read block data (block %d)\n",
					__func__, block_num);
			goto exit;
		}

		index += fwu->block_size;
	}

exit:
	fwu->rmi4_data->reset_device(fwu->rmi4_data);

	return retval;
}

/*
static int fwu_do_lockdown(void)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	retval = fwu_enter_flash_prog();
	if (retval < 0)
		return retval;

	retval = fwu->fn_ptr->read(rmi4_data,
			fwu->f34_fd.query_base_addr + fwu->properties_off,
			fwu->flash_properties.data,
			sizeof(fwu->flash_properties.data));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read flash properties\n",
				__func__);
		return retval;
	}

	if (fwu->flash_properties.unlocked == 0) {
		tsp_debug_info(true, &rmi4_data->i2c_client->dev,
				"%s: Device already locked down\n",
				__func__);
		return retval;
	}

	retval = fwu_write_lockdown_block();
	if (retval < 0)
		return retval;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
				"%s: Lockdown programmed\n", __func__);

	return retval;
}
*/

int synaptics_fw_updater(unsigned char *fw_data)
{
#ifdef USE_REWRITE_TEST_RESULT
	int before_test_result = 0, after_test_result = 0;
#endif
	int retval;

	if (!fwu)
		return -ENODEV;

	if (!fwu->initialized)
		return -ENODEV;

	if (!fw_data) {
		tsp_debug_err(true, &fwu->rmi4_data->i2c_client->dev,
				"%s: Firmware data is NULL\n", __func__);
		return -ENODEV;
	}

	fwu->ext_data_source = fw_data;
	fwu->config_area = UI_CONFIG_AREA;

#ifdef USE_REWRITE_TEST_RESULT
	before_test_result = synaptics_rmi4_read_tsp_test_result(rmi4_data);
#endif

	retval = fwu_start_reflash();
	if (retval < 0)
		goto out_fw_update;

#ifdef USE_REWRITE_TEST_RESULT
	/* TODO: I do not recommend below codes. because to set the test result
	 * is not verified completed. and below code is not efficent when firmware update is
	 * broken by unintended enviroment such as power off.
	 */
	if (!before_test_result)
		goto out_fw_update;

	after_test_result = synaptics_rmi4_read_tsp_test_result(rmi4_data);

	if (before_test_result != after_test_result) {
		tsp_debug_info(true, &fwu->rmi4_data->i2c_client->dev, "%s: Re-write test result after firmware update before[%x]/after[%x]\n",
				__func__, before_test_result, after_test_result);

		retval = synaptics_rmi4_set_tsp_test_result_in_config(before_test_result);
		if (retval < 0)
			tsp_debug_err(true, &fwu->rmi4_data->i2c_client->dev, "%s: Failed to write tsp_test_result in config\n",
					__func__);
	}
#endif
out_fw_update:
	return retval;
}
EXPORT_SYMBOL(synaptics_fw_updater);

static ssize_t fwu_sysfs_show_image(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (count < fwu->config_size) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Not enough space (%ld bytes) in buffer\n",
				__func__, count);
		return -EINVAL;
	}

	memcpy(buf, fwu->read_config_buf, fwu->config_size);

	return fwu->config_size;
}

static ssize_t fwu_sysfs_store_image(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	memcpy((void *)(&fwu->ext_data_source[fwu->data_pos]),
			(const void *)buf,
			count);

	fwu->data_pos += count;
	fwu->img.fw_image = fwu->ext_data_source;
	return count;
}

static ssize_t fwu_sysfs_do_reflash_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (sscanf(buf, "%u", &input) != 1) {
		retval = -EINVAL;
		goto exit;
	}

	if (input & UPDATE_MODE_LOCKDOWN) {
		fwu->do_lockdown = true;
		input &= ~UPDATE_MODE_LOCKDOWN;
	}

	if ((input != UPDATE_MODE_NORMAL) && (input != UPDATE_MODE_FORCE)) {
		retval = -EINVAL;
		goto exit;
	}

	if (input == UPDATE_MODE_FORCE)
		fwu->force_update = true;

	retval = synaptics_fw_updater(fwu->ext_data_source);
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to do reflash\n",
				__func__);
		goto exit;
	}

	retval = count;

exit:
	kfree(fwu->ext_data_source);
	fwu->ext_data_source = NULL;
	fwu->force_update = FORCE_UPDATE;
	fwu->do_lockdown = DO_LOCKDOWN;
	return retval;
}

static ssize_t fwu_sysfs_write_config_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (sscanf(buf, "%u", &input) != 1) {
		retval = -EINVAL;
		goto exit;
	}

	if (input != 1) {
		retval = -EINVAL;
		goto exit;
	}

	retval = fwu_start_write_config();
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write config\n",
				__func__);
		goto exit;
	}

	retval = count;

exit:
	kfree(fwu->ext_data_source);
	fwu->ext_data_source = NULL;
	return retval;
}

static ssize_t fwu_sysfs_read_config_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input != 1)
		return -EINVAL;

	retval = fwu_do_read_config();
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read config\n",
				__func__);
		return retval;
	}

	return count;
}

static ssize_t fwu_sysfs_config_area_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long config_area;

	retval = kstrtoul(buf, 10, &config_area);
	if (retval)
		return retval;

	fwu->config_area = config_area;

	return count;
}

/*
static ssize_t fwu_sysfs_image_name_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	memcpy(fwu->img.image_name, buf, count);

	return count;
}
*/

static ssize_t fwu_sysfs_image_size_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long size;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	retval = kstrtoul(buf, 10, &size);
	if (retval)
		return retval;

	fwu->img.image_size = size;
	fwu->data_pos = 0;

	kfree(fwu->ext_data_source);
	fwu->ext_data_source = kzalloc(fwu->img.image_size, GFP_KERNEL);
	if (!fwu->ext_data_source) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for image data\n",
				__func__);
		return -ENOMEM;
	}

	return count;
}

static ssize_t fwu_sysfs_block_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", fwu->block_size);
}

static ssize_t fwu_sysfs_firmware_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", fwu->fw_block_count);
}

static ssize_t fwu_sysfs_configuration_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", fwu->config_block_count);
}

static ssize_t fwu_sysfs_perm_config_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", fwu->perm_config_block_count);
}

static ssize_t fwu_sysfs_bl_config_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", fwu->bl_config_block_count);
}

static ssize_t fwu_sysfs_disp_config_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", fwu->disp_config_block_count);
}

static ssize_t fwu_sysfs_guest_code_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", fwu->guest_code_block_count);
}

static ssize_t fwu_sysfs_write_guest_code_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (sscanf(buf, "%u", &input) != 1) {
		retval = -EINVAL;
		goto exit;
	}

	if (input != 1) {
		retval = -EINVAL;
		goto exit;
	}

	retval = fwu_start_write_guest_code();
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write guest code\n",
				__func__);
		goto exit;
	}

	retval = count;

exit:
	kfree(fwu->ext_data_source);
	fwu->ext_data_source = NULL;
	return retval;
}

static void synaptics_rmi4_fwu_attn(struct synaptics_rmi4_data *rmi4_data,
		unsigned char intr_mask)
{
	if (!fwu)
		return;

	if (fwu->intr_mask & intr_mask)
		fwu_read_f34_flash_status();

	return;
}

static void fwu_img_scan_x10_container(unsigned int listLength, unsigned char *startAddr)
{
	unsigned int i;
	unsigned int length;
	unsigned int contentAddr, addr;
	unsigned short containerId;
	struct block_data block;
	struct img_x10_descriptor *containerDescriptor;
	struct img_x10_bl_container *blContainer;

	for (i = 0; i < listLength; i += 4) {
		contentAddr = extract_uint_le(startAddr + i);
		containerDescriptor =
			(struct img_x10_descriptor *)(fwu->img.fw_image + contentAddr);
		containerId = extract_ushort_le(containerDescriptor->containerID);
		addr = extract_uint_le(containerDescriptor->contentAddress);
		length = extract_uint_le(containerDescriptor->contentLength);
		block.data = fwu->img.fw_image + addr;
		block.size = length;
		switch (containerId) {
		case ID_UI_CONTAINER:
			fwu->img.uiFirmware = block;
			break;
		case ID_UI_CONFIGURATION:
			fwu->img.uiConfig = block;
			fwu->img.configId = fwu->img.uiConfig.data;
			break;
		case ID_BOOTLOADER_LOCKDOWN_INFORMATION_CONTAINER:
			fwu->img.lockdown = block;
			break;
		case ID_GUEST_CODE_CONTAINER:
			fwu->img.guestCode = block;
			break;
		case ID_BOOTLOADER_CONTAINER:
			blContainer =
			(struct img_x10_bl_container *)(fwu->img.fw_image + addr);
			fwu->img.blMajorVersion = blContainer->majorVersion;
			fwu->img.blMinorVersion = blContainer->minorVersion;
			fwu->img.bootloaderInfo = block;
			break;
		case ID_PERMANENT_CONFIGURATION_CONTAINER:
			fwu->img.permanent = block;
			break;
		case ID_GENERAL_INFORMATION_CONTAINER:
			fwu->img.packageId = fwu->img.fw_image + addr + 0;
			fwu->img.firmwareId = fwu->img.fw_image + addr + 4;
			fwu->img.dsFirmwareInfo = fwu->img.fw_image + addr + 8;
			break;
		default:
			break;
		}
	}
}

static void fwu_img_parse_x10_topcontainer(void)
{
	struct img_x10_descriptor *descriptor;
	unsigned int topAddr;
	unsigned int listLength, blLength;
	unsigned char *startAddr;

	topAddr = extract_uint_le(fwu->img.fw_image +
				IMG_X10_TOP_CONTAINER_OFFSET);
	descriptor = (struct img_x10_descriptor *)
			(fwu->img.fw_image + topAddr);
	listLength = extract_uint_le(descriptor->contentLength);
	startAddr = fwu->img.fw_image +
			extract_uint_le(descriptor->contentAddress);
	fwu_img_scan_x10_container(listLength, startAddr);
	/* scan sub bootloader container (lockdown container) */
	if (fwu->img.bootloaderInfo.data != NULL) {
		blLength = fwu->img.bootloaderInfo.size - 4;
		if (blLength)
			fwu_img_scan_x10_container(blLength,
					fwu->img.bootloaderInfo.data);
	}
}

static void fwu_img_parse_x10(void)
{
	fwu_img_parse_x10_topcontainer();
}

static void fwu_img_parse_x0_x6(void)
{
	struct img_x0x6_header *header = (struct img_x0x6_header *)fwu->img.fw_image;
	if (header->bootloader_version > 6)
		return;
	fwu->img.blMajorVersion = header->bootloader_version;
	fwu->img.uiFirmware.size = extract_uint_le(header->firmware_size);
	fwu->img.uiFirmware.data = fwu->img.fw_image + IMG_X0_X6_FW_OFFSET;
	fwu->img.uiConfig.size = extract_uint_le(header->config_size);
	fwu->img.uiConfig.data = fwu->img.uiFirmware.data + fwu->img.uiFirmware.size;
	fwu->img.configId = fwu->img.uiConfig.data;
	switch (fwu->img.imageFileVersion) {
	case 0x2:
		fwu->img.lockdown.size = 0x30;
		break;
	case 0x3:
	case 0x4:
		fwu->img.lockdown.size = 0x40;
		break;
	case 0x5:
	case 0x6:
		fwu->img.lockdown.size = 0x50;
		if (header->options_firmware_id) {
			fwu->img.firmwareId = header->firmware_id;
			fwu->img.packageId = header->package_id;
			fwu->img.dsFirmwareInfo = header->ds_firmware_info;
		}
		break;
	default:
		break;
	}
	fwu->img.lockdown.data = fwu->img.fw_image +
		IMG_X0_X6_FW_OFFSET - fwu->img.lockdown.size;
}

static void fwu_img_parse_format(void)
{
	fwu->polling_mode = POLLING_MODE_DEFAULT;
	fwu->img.firmwareId = NULL;
	fwu->img.packageId = NULL;
	fwu->img.dsFirmwareInfo = NULL;
	fwu->img.uiFirmware.data = NULL;
	fwu->img.uiConfig.data = NULL;
	fwu->img.lockdown.data = NULL;
	fwu->img.guestCode.data = NULL;
	fwu->img.uiConfig.size = 0;
	fwu->img.uiFirmware.size = 0;
	fwu->img.lockdown.size = 0;
	fwu->img.guestCode.size =	 0;

	fwu->img.imageFileVersion = fwu->img.fw_image[IMG_VERSION_OFFSET];

	switch (fwu->img.imageFileVersion) {
	case 0x10:
		fwu_img_parse_x10();
		break;
	case 0x5:
	case 0x6:
		fwu_img_parse_x0_x6();
		break;
	default:
		tsp_debug_err(true, &fwu->rmi4_data->i2c_client->dev,
				"%s: Unsupported image file format $%X\n",
				__func__, fwu->img.imageFileVersion);
		break;
	}
}

static int synaptics_rmi4_fwu_init(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char attr_count;
	int attr_count_num;
	struct pdt_properties pdt_props;

	fwu = kzalloc(sizeof(*fwu), GFP_KERNEL);
	if (!fwu) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for fwu\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	fwu->fn_ptr = kzalloc(sizeof(*(fwu->fn_ptr)), GFP_KERNEL);
	if (!fwu->fn_ptr) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for fn_ptr\n",
				__func__);
		retval = -ENOMEM;
		goto exit_free_fwu;
	}

	memset(&fwu->img, 0, sizeof(fwu->img));

	fwu->img.image_name = kzalloc(MAX_IMAGE_NAME_LEN, GFP_KERNEL);
	if (!fwu->img.image_name) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for image name\n",
				__func__);
		retval = -ENOMEM;
		goto exit_free_fn_ptr;
	}

	fwu->rmi4_data = rmi4_data;
	fwu->fn_ptr->read = rmi4_data->i2c_read;
	fwu->fn_ptr->write = rmi4_data->i2c_write;
	fwu->fn_ptr->enable = rmi4_data->irq_enable;

	retval = fwu->fn_ptr->read(rmi4_data,
			PDT_PROPS,
			pdt_props.data,
			sizeof(pdt_props.data));
	if (retval < 0) {
		tsp_debug_info(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read PDT properties, assuming 0x00\n",
				__func__);
	} else if (pdt_props.has_bsr) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Reflash for LTS not currently supported\n",
				__func__);
		retval = -ENODEV;
		goto exit_free_mem;
	}

	retval = fwu_scan_pdt();
	if (retval < 0)
		goto exit_free_mem;

	fwu->productinfo1 = rmi4_data->rmi4_mod_info.product_info[0];
	fwu->productinfo2 = rmi4_data->rmi4_mod_info.product_info[1];
	memcpy(fwu->product_id, rmi4_data->rmi4_mod_info.product_id_string,
			SYNAPTICS_RMI4_PRODUCT_ID_SIZE);
	fwu->product_id[SYNAPTICS_RMI4_PRODUCT_ID_SIZE] = 0;

	retval = fwu_read_f34_queries();
	if (retval < 0)
		goto exit_free_mem;

	fwu->force_update = FORCE_UPDATE;
	fwu->do_lockdown = DO_LOCKDOWN;
	fwu->initialized = true;

	retval = sysfs_create_bin_file(&rmi4_data->input_dev->dev.kobj,
			&dev_attr_data);
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to create sysfs bin file\n",
				__func__);
		goto exit_free_mem;
	}

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = sysfs_create_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to create sysfs attributes\n",
					__func__);
			retval = -ENODEV;
			goto exit_remove_attrs;
		}
	}

	return 0;

exit_remove_attrs:
	attr_count_num = (int)attr_count;
	for (attr_count_num--; attr_count_num >= 0; attr_count_num--) {
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
	}

	sysfs_remove_bin_file(&rmi4_data->input_dev->dev.kobj, &dev_attr_data);

exit_free_mem:
	kfree(fwu->img.image_name);

exit_free_fn_ptr:
	kfree(fwu->fn_ptr);

exit_free_fwu:
	kfree(fwu);
	fwu = NULL;

exit:
	return retval;
}

static void synaptics_rmi4_fwu_remove(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned char attr_count;

	if (!fwu)
		goto exit;

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
	}

	sysfs_remove_bin_file(&rmi4_data->input_dev->dev.kobj, &dev_attr_data);

	kfree(fwu->read_config_buf);
	kfree(fwu->img.image_name);
	kfree(fwu);
	fwu = NULL;

exit:
	return;
}

int rmi4_fw_update_module_register(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;

	retval = synaptics_rmi4_new_function(RMI_FW_UPDATER,
			rmi4_data,
			synaptics_rmi4_fwu_init,
			NULL,
			synaptics_rmi4_fwu_remove,
			synaptics_rmi4_fwu_attn);

	return retval;
}
