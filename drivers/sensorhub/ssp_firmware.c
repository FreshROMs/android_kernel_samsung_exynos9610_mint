/*
 *  Copyright (C) 2019, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include "ssp_firmware.h"

#include "factory/ssp_sensor.h"
#include "ssp.h"

#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

enum fw_type {
	FW_TYPE_NONE,
	FW_TYPE_BIN,
	FW_TYPE_PUSH,
	FW_TYPE_SPU,
};

//#define SUPPORT_SPU_FW
#define UPDATE_BIN_PATH	   "/vendor/firmware/shub.bin"
#define UPDATE_BIN_FW_NAME "shub.bin"

#ifdef SUPPORT_SPU_FW
#define SPU_FW_FILE "/spu/sensorhub/shub_spu.bin"

#define FW_VER_LEN 8
extern long spu_firmware_signature_verify(const char *fw_name, const u8 *fw_data, const long fw_size);

static int request_spu_firmware(struct ssp_data *data, u8 **fw_buf)
{
	int ret = 0;
	size_t file_size = 0, remaining;
	int offset = 0;
	unsigned int read_size = PAGE_SIZE * 10;
	long fw_size = 0;
	struct file *filp = NULL;
	u8 *file_buf;

	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	ssp_infof("");

	filp = filp_open(SPU_FW_FILE, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		ssp_infof("filp_open failed %d", PTR_ERR(filp));
		set_fs(old_fs);
		return 0;
	}

	file_size = filp->f_path.dentry->d_inode->i_size;
	if (file_size <= 0) {
		filp_close(filp, NULL);
		set_fs(old_fs);
		return 0;
	}

	file_buf = kvzalloc(file_size, GFP_KERNEL);
	if (file_buf == NULL) {
		ssp_errf("file buf kvzalloc error");
		return 0;
	}

	remaining = file_size;

	while (remaining > 0) {
		int ret = 0;
		if (read_size > remaining) {
			read_size = remaining;
		}

		ret = (unsigned int)vfs_read(filp, (char __user *)(file_buf + offset), read_size, &filp->f_pos);

		if (ret != read_size) {
			ssp_errf("file read fail %d", ret);
			break;
		}

		offset += read_size;
		remaining -= read_size;
		filp->f_pos = offset;
	}

	filp_close(filp, NULL);
	set_fs(old_fs);
	if (ret < 0) {
		ssp_errf("file read fail %d", ret);
		kfree(file_buf);
		return 0;
	}

	// check signing
	fw_size = spu_firmware_signature_verify("SENSORHUB", file_buf, file_size);
	if (fw_size < 0) {
		ssp_errf("signature verification failed %d", fw_size);
		fw_size = 0;
	} else {
		u32 fw_version = 0;
		char str_ver[9] = "";

		ssp_infof("signature verification success %d", fw_size);
		if (fw_size < FW_VER_LEN) {
			ssp_errf("fw size is wrong %d", fw_size);
			kfree(file_buf);
			return 0;
		}

		memcpy(str_ver, file_buf + fw_size - FW_VER_LEN, 8);

		ret = kstrtou32(str_ver, 10, &fw_version);
		ssp_infof("urgent fw_version %d kernel ver %u", fw_version, get_module_rev(data));

		if (fw_version > get_module_rev(data)) {
			fw_size -= FW_VER_LEN;
			ssp_infof("use spu fw size %d", fw_size);
			*fw_buf = kvzalloc(fw_size, GFP_KERNEL);
			if (*fw_buf == NULL) {
				ssp_errf("fw buf kvzalloc error");
				kfree(file_buf);
				return 0;
			}
			memcpy(*fw_buf, file_buf, fw_size);
		} else
			fw_size = 0;
	}
	kfree(file_buf);

	return (int)fw_size;
}

static void release_spu_firmware(struct ssp_data *data, u8 *fw_buf)
{
	kfree(fw_buf);
}
#endif

#ifdef CONFIG_SSP_ENG_DEBUG
static bool is_exist_force_update_fw(void)
{
	bool is_exist = false;
	mm_segment_t old_fs;
	struct file *fw_file = NULL;

	ssp_infof("check push bin file %s", UPDATE_BIN_PATH);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fw_file = filp_open(UPDATE_BIN_PATH, O_RDONLY, 0660);
	if (IS_ERR(fw_file)) {
		ssp_errf("error %d", PTR_ERR(fw_file));
		is_exist = false;
	} else {
		is_exist = true;
	}

	set_fs(old_fs);
	return is_exist;
}
#endif

int download_sensorhub_firmware(struct ssp_data *data, struct device *dev, void *addr)
{
	int ret = 0;
	int fw_size;
	char *fw_buf = NULL;
	const struct firmware *entry = NULL;

	if (addr == NULL)
		return -EINVAL;

#ifdef CONFIG_SSP_ENG_DEBUG
	if (is_exist_force_update_fw()) {
		ret = request_firmware(&entry, UPDATE_BIN_FW_NAME, dev);
		if (!ret) {
			data->fw_type = FW_TYPE_PUSH;
			fw_size = (int)entry->size;
			fw_buf = (char *)entry->data;
		}
	} else
#endif
	{
#ifdef SUPPORT_SPU_FW
		fw_size = request_spu_firmware(data, (u8 **)&fw_buf);
		if (fw_size > 0) {
			ssp_infof("dowload spu firmware");
			data->fw_type = FW_TYPE_SPU;
		} else
#endif
		{
			ssp_infof("download %s", data->fw_name);
			ret = request_firmware(&entry, data->fw_name, dev);
			if (ret) {
				ssp_errf("request_firmware failed %d", ret);
				data->fw_type = FW_TYPE_NONE;
				release_firmware(entry);
				return -EINVAL;
			}

			data->fw_type = FW_TYPE_BIN;
			fw_size = (int)entry->size;
			fw_buf = (char *)entry->data;
		}
	}

	ssp_infof("fw type %d bin(size:%d) on %lx", data->fw_type, (int)fw_size, (unsigned long)addr);

	memcpy(addr, fw_buf, fw_size);

	if (entry)
		release_firmware(entry);

#ifdef SUPPORT_SPU_FW
	if (data->fw_type == FW_TYPE_SPU)
		release_spu_firmware(data, fw_buf);
#endif
	return 0;
}
