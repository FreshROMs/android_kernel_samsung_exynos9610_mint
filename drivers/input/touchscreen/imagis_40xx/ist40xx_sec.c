/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
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

#include <linux/vmalloc.h>

#include "ist40xx.h"
#include "ist40xx_update.h"
#include "ist40xx_misc.h"
#include "ist40xx_cmcs.h"

#ifdef SEC_FACTORY_MODE

#if defined(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#endif

#define COMMAND_LENGTH		(64)
#define FACTORY_BUF_SIZE	PAGE_SIZE
#define BUILT_IN		(0)
#define UMS			(1)

#define TSP_CH_UNKNOWN		(0)
#define TSP_CH_UNUSED		(1)
#define TSP_CH_USED		(2)

#define ORIGIN_ARRAY		{ LTR_TTB, TTB_LTR, RTL_TTB, TTB_RTL, \
					LTR_BTT, BTT_LTR, RTL_TTB, BTT_RTL }

int ist40xx_origin_value[ORI_MAX] = ORIGIN_ARRAY;

u32 ist40xx_get_fw_ver(struct ist40xx_data *data)
{
	u32 ver = 0;
	int ret = 0;

	ret = ist40xx_read_cmd(data, eHCOM_GET_VER_FW, &ver);
	if (ret) {
		ist40xx_reset(data, false);
		ist40xx_start(data);

		input_info(true, &data->client->dev, "%s: ret=%d\n", __func__, ret);
		return ver;
	}

	tsp_debug("Reg addr: %x, ver: %x\n", eHCOM_GET_VER_FW, ver);

	return ver;
}

u32 ist40xx_get_fw_chksum(struct ist40xx_data *data)
{
	u32 chksum = 0;
	int ret = 0;

	ret = ist40xx_read_cmd(data, eHCOM_GET_CRC32, &chksum);
	if (ret) {
		ist40xx_reset(data, false);
		ist40xx_start(data);

		input_err(true, &data->client->dev, "%s: ret=%d\n", __func__, ret);
		return 0;
	}

	tsp_debug("Reg addr: 0x%08x, chksum: %08x\n", eHCOM_GET_CRC32, chksum);

	return chksum;
}

static void not_support_cmd(void *dev_data)
{
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	snprintf(buf, sizeof(buf), "NA");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void get_chip_vendor(void *dev_data)
{
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	snprintf(buf, sizeof(buf), "%s", TSP_CHIP_VENDOR);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)),
					   "IC_VENDOR");
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void get_chip_name(void *dev_data)
{
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	snprintf(buf, sizeof(buf), "%s", TSP_CHIP_NAME);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)),
					   "IC_NAME");
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void get_chip_id(void *dev_data)
{
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	snprintf(buf, sizeof(buf), "%#02x", data->chip_id);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

#include <linux/uaccess.h>
#define MAX_FW_PATH		255
static void fw_update(void *dev_data)
{
	int ret;
	char buf[16] = { 0 };
	mm_segment_t old_fs = { 0 };
	struct file *fp = NULL;
	long fsize = 0, nread = 0;
	char fw_path[MAX_FW_PATH + 1];
	u8 *fwbuf = NULL;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);
#if defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	if (sec->cmd_param[0] == 1) {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buf, sizeof(buf), "OK");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		input_info(true, &data->client->dev, "%s: user_ship binary, success\n", __func__);
		return;
	}
#endif

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s() now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);

		snprintf(buf, sizeof(buf), "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		return;
	}

	switch (sec->cmd_param[0]) {
	case BUILT_IN:
		sec->cmd_state = SEC_CMD_STATUS_OK;

		ret = ist40xx_fw_recovery(data);
		if (ret < 0)
			sec->cmd_state = SEC_CMD_STATUS_FAIL;

		break;
	case UMS:
		sec->cmd_state = SEC_CMD_STATUS_OK;

		fwbuf = vzalloc(IST40XX_ROM_TOTAL_SIZE + sizeof(struct ist40xx_tags));
		if (!fwbuf) {
			input_err(true, &data->client->dev,
				  "%s() failed to fwbuf allocate\n", __func__);

			sec->cmd_state= SEC_CMD_STATUS_FAIL;

			break;
		}

		old_fs = get_fs();
		set_fs(get_ds());

		snprintf(fw_path, MAX_FW_PATH, "/sdcard/Firmware/TSP/%s", IST40XX_FW_NAME);
		fp = filp_open(fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			input_err(true, &data->client->dev, "%s: file %s open error:%d\n",
				  __func__, fw_path, IS_ERR(fp));

			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			set_fs(old_fs);

			break;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		if (fsize != data->fw.buf_size) {
			input_err(true, &data->client->dev, "%s: invalid fw size!!\n",
				  __func__);

			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			set_fs(old_fs);

			break;
		}

		nread = vfs_read(fp, (char __user *)fwbuf, fsize, &fp->f_pos);
		if (nread != fsize) {
			input_err(true, &data->client->dev, "%s: failed to read fw\n",
				  __func__);

			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			filp_close(fp, NULL);
			set_fs(old_fs);

			break;
		}

		filp_close(fp, current->files);
		set_fs(old_fs);

		input_info(true, &data->client->dev, "%s: ums fw is loaded!!\n",
			   __func__);

		ret = ist40xx_get_update_info(data, fwbuf, fsize);
		if (ret) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			break;
		}

		mutex_lock(&data->lock);
		ret = ist40xx_fw_update(data, fwbuf, fsize);
		if (ret) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			mutex_unlock(&data->lock);
			break;
		}
		ist40xx_print_info(data);
#ifdef TCLM_CONCEPT
		sec_tclm_root_of_cal(data->tdata, CALPOSITION_TESTMODE);

		ret = sec_execute_tclm_package(data->tdata, 0);
		if (ret < 0)
			input_err(true, &data->client->dev,
				  "%s: sec_execute_tclm_package fail\n", __func__);

		sec_tclm_root_of_cal(data->tdata, CALPOSITION_NONE);
#else
		ist40xx_calibrate(data, 1);
#endif
		mutex_unlock(&data->lock);
		ist40xx_start(data);

		break;

	default:
		input_err(true, &data->client->dev, "%s() Invalid fw file type!\n",
			  __func__);
		break;
	}

	if (fwbuf)
		vfree(fwbuf);

	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buf, sizeof(buf), "OK");
	else
		snprintf(buf, sizeof(buf), "NG");

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
}

static void get_fw_ver_bin(void *dev_data)
{
	u32 ver = 0;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->dt_data->bringup != 1) {
		ver = ist40xx_parse_ver(data, FLAG_FW, data->fw.buf);
	}
	snprintf(buf, sizeof(buf), "IM%08X", ver);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)),
					   "FW_VER_BIN");
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void get_config_ver(void *dev_data)
{
	char buf[255] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	snprintf(buf, sizeof(buf), "%s_%s", TSP_CHIP_VENDOR, TSP_CHIP_NAME);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void get_checksum_data(void *dev_data)
{
	char buf[16] = { 0 };
	u32 chksum = 0;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_OFF)
		chksum = ist40xx_get_fw_chksum(data);

	if (chksum == 0) {
		input_err(true, &data->client->dev,
			  "%s: Failed get the checksum data \n", __func__);

		snprintf(buf, sizeof(buf), "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		return;
	}

	snprintf(buf, sizeof(buf), "0x%06X", chksum);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void get_fw_ver_ic(void *dev_data)
{
	u32 ver = 0;
	char msg[8];
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_OFF) {
		ver = ist40xx_get_fw_ver(data);
		snprintf(buf, sizeof(buf), "IM%08X", ver);
	} else {
		snprintf(buf, sizeof(buf), "IM%08X", data->fw.cur.fw_ver);
	}

	if (data->fw.cur.test_ver > 0) {
		sprintf(msg, "(T%X)", data->fw.cur.test_ver);
		strcat(buf, msg);
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)),
					   "FW_VER_IC");
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void dead_zone_enable(void *dev_data)
{
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);

		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		goto out;
	}

	switch (sec->cmd_param[0]) {
	case 0:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &data->client->dev, "%s: Set Edge Mode\n",
			   __func__);

		ist40xx_set_edge_mode(1);

		break;
	case 1:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &data->client->dev, "%s: Unset Edge Mode\n",
			   __func__);

		ist40xx_set_edge_mode(0);

		break;
	default:
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_info(true, &data->client->dev, "%s: Invalid Argument\n",
			   __func__);

		break;
	}

out:
	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buf, sizeof(buf), "OK");
	else
		snprintf(buf, sizeof(buf), "NG");

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void call_mode(void *dev_data)
{
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);

		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		goto out;
	}

	switch (sec->cmd_param[0]) {
	case 0:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &data->client->dev, "%s: Unset Call Mode\n",
			   __func__);

		ist40xx_set_call_mode(0);

		break;
	case 1:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &data->client->dev, "%s: Set Call Mode\n",
			   __func__);

		ist40xx_set_call_mode(1);

		break;
	default:
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_info(true, &data->client->dev, "%s: Invalid Argument\n",
			   __func__);

		break;
	}

out:
	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buf, sizeof(buf), "%s", "OK");
	else
		snprintf(buf, sizeof(buf), "%s", "NG");

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void glove_mode(void *dev_data)
{
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);

		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		goto out;
	}

	switch (sec->cmd_param[0]) {
	case 0:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &data->client->dev, "%s: Unset Glove Mode\n",
			   __func__);

		ist40xx_set_glove_mode(0);

		break;
	case 1:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &data->client->dev, "%s: Set Glove Mode\n",
			   __func__);

		ist40xx_set_glove_mode(1);

		break;
	default:
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_info(true, &data->client->dev, "%s: Invalid Argument\n",
			   __func__);

		break;
	}

out:
	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buf, sizeof(buf), "%s", "OK");
	else
		snprintf(buf, sizeof(buf), "%s", "NG");

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

/*
 *	index
 *		0 :  set edge handler
 *		1 :  portrait (normal) mode
 *		2 :  landscape mode
 *
 *	data
 *		0, X (direction), X (y start), X (y end)
 *		direction : 0 (off), 1 (left), 2 (right)
 *			ex) echo set_grip_data,0,2,600,900 > cmd
 *
 *		1, X (edge zone), X (dead zone up x), X (dead zone down x), X (dead zone y)
 *			ex) echo set_grip_data,1,200,10,50,1500 > cmd
 *
 *		2, 1 (landscape mode), X (edge zone), X (dead zone x), X (dead zone top y), X (dead zone bottom y)
 *			ex) echo set_grip_data,2,1,200,100,120,0 > cmd
 *
 *		2, 0 (landscape mode off)
 *			ex) echo set_grip_data,2,0 > cmd
 */
static void set_grip_data(void *dev_data)
{
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);

		goto err_grip_data;
	}

	if (sec->cmd_param[0] == 2) {	// landscape mode
		if (sec->cmd_param[1] == 0) {   // normal mode
			sec->cmd_state = SEC_CMD_STATUS_OK;
			input_info(true, &data->client->dev,
				  "%s: Unset Touchable Area\n", __func__);

			ist40xx_set_rejectzone_mode(0, 0, 0);
		} else if (sec->cmd_param[1] == 1) {
			input_info(true, &data->client->dev,
				  "%s: Set Touchable Area\n", __func__);

			ist40xx_set_rejectzone_mode(1, sec->cmd_param[4],
					sec->cmd_param[5]);
		} else {
			input_info(true, &data->client->dev,
				   "%s: not support function\n", __func__);
		}
	} else {
		input_info(true, &data->client->dev, "%s: not support function\n", __func__);
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;

	snprintf(buf, sizeof(buf), "OK");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));

	return;

err_grip_data:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	snprintf(buf, sizeof(buf), "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void set_touchable_area(void *dev_data)
{
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);

		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		goto out;
	}

	switch (sec->cmd_param[0]) {
	case 0:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &data->client->dev, "%s: Unset Touchable Area\n",
			   __func__);

		ist40xx_set_touchable_mode(0);

		break;
	case 1:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &data->client->dev, "%s: Set Touchable Area\n",
			   __func__);

		ist40xx_set_touchable_mode(1);

		break;
	default:
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_info(true, &data->client->dev, "%s: Invalid Argument\n",
			   __func__);

		break;
	}

out:
	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buf, sizeof(buf), "OK");
	else
		snprintf(buf, sizeof(buf), "NG");

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &data->client->dev, "%s() %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void spay_enable(void *dev_data)
{
	int ret;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	switch (sec->cmd_param[0]) {
	case 0:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &data->client->dev, "%s: Unset SPAY Mode\n",
			   __func__);

		data->lpm_mode &= ~IST40XX_SPAY;

		break;
	case 1:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &data->client->dev, "%s: Set SPAY Mode\n",
			   __func__);

		data->lpm_mode |= IST40XX_SPAY;

		break;
	default:
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_info(true, &data->client->dev, "%s: Invalid Argument\n",
			   __func__);

		break;
	}

	if (data->status.sys_mode != STATE_POWER_OFF) {
		ret = ist40xx_write_sponge_reg(data, IST40XX_SPONGE_CTRL,
				(u16*)&data->lpm_mode, 1, true);
		if (ret) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			input_err(true, &data->client->dev,
				   "%s: fail to write sponge reg\n", __func__);

			goto err;
		}

		ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
				(eHCOM_NOTIRY_G_REGMAP << 16) | IST40XX_ENABLE);
		if (ret) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			input_err(true, &data->client->dev,
				    "%s: fail to write notify packet.\n", __func__);

			goto err;
		}
	}

err:
	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buf, sizeof(buf), "OK");
	else
		snprintf(buf, sizeof(buf), "NG");

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void aot_enable(void *dev_data)
{
	int ret;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	switch (sec->cmd_param[0]) {
	case 0:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &data->client->dev, "%s: Unset AOT Mode\n",
			__func__);

		data->lpm_mode &= ~IST40XX_DOUBLETAP_WAKEUP;

		break;
	case 1:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &data->client->dev, "%s: Set AOT Mode\n",
			__func__);

		data->lpm_mode |= IST40XX_DOUBLETAP_WAKEUP;

		break;
	default:
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_info(true, &data->client->dev, "%s: Invalid Argument\n",
			__func__);

		break;
	}

	if (data->status.sys_mode != STATE_POWER_OFF) {
		ret = ist40xx_write_sponge_reg(data, IST40XX_SPONGE_CTRL,
				(u16*)&data->lpm_mode, 1, true);
		if (ret) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			input_err(true, &data->client->dev,
				"%s: fail to write sponge reg.\n", __func__);

			goto err;
		}

		ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
				(eHCOM_NOTIRY_G_REGMAP << 16) | IST40XX_ENABLE);
		if (ret) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			input_err(true, &data->client->dev,
				"%s: fail to write notify packet.\n", __func__);

			goto err;
		}
	}

err:
	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buf, sizeof(buf), "OK");
	else
		snprintf(buf, sizeof(buf), "NG");

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
			(int)strnlen(buf, sizeof(buf)));
}

static void aod_enable(void *dev_data)
{
	int ret;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	switch (sec->cmd_param[0]) {
	case 0:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &data->client->dev, "%s: Unset AOD Mode\n",
			   __func__);

		data->lpm_mode &= ~IST40XX_AOD;

		break;
	case 1:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &data->client->dev, "%s: Set AOD Mode\n",
			   __func__);

		data->lpm_mode |= IST40XX_AOD;

		break;
	default:
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_info(true, &data->client->dev, "%s: Invalid Argument\n",
			   __func__);

		break;
	}

	if (data->status.sys_mode != STATE_POWER_OFF) {
		ret = ist40xx_write_sponge_reg(data, IST40XX_SPONGE_CTRL,
				(u16*)&data->lpm_mode, 1, true);
		if (ret) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			input_err(true, &data->client->dev,
				  "%s: fail to write sponge reg.\n", __func__);

			goto err;
		}

		ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
				(eHCOM_NOTIRY_G_REGMAP << 16) | IST40XX_ENABLE);
		if (ret) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			input_err(true, &data->client->dev,
				   "%s: fail to write notify packet.\n", __func__);

			goto err;
		}
	}

err:
	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buf, sizeof(buf), "OK");
	else
		snprintf(buf, sizeof(buf), "NG");

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void set_aod_rect(void *dev_data)
{
	int i;
	int ret;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	for (i = 0; i < 4; i++)
		data->rect_data[i] = sec->cmd_param[i];
	
	if (data->status.sys_mode == STATE_LPM) {
		ret = ist40xx_burst_write(data->client, IST40XX_HIB_SPONGE_RECT,
						(u32 *)data->rect_data, 2);
		if (ret) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			input_err(true, &data->client->dev,
						"%s: fail to write rect\n", __func__);

			goto err_rect;
		}

		ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
						(eHCOM_SET_AOD_RECT << 16) | (IST40XX_ENABLE & 0xFFFF));
		if (ret) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			input_err(true, &data->client->dev,
						"%s: fail to write rect notify.\n", __func__);

			goto err_rect;
		}
	}

err_rect:
	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buf, sizeof(buf), "OK");
	else
		snprintf(buf, sizeof(buf), "NG");

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void get_aod_rect(void *dev_data)
{
	char buf[32] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	snprintf(buf, sizeof(buf), "%d,%d,%d,%d", data->rect_data[0],
			data->rect_data[1], data->rect_data[2], data->rect_data[3]);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void singletap_enable(void *dev_data)
{
	int ret;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	switch (sec->cmd_param[0]) {
	case 0:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &data->client->dev, "%s: Unset SingleTap Mode\n",
			   __func__);

		data->lpm_mode &= ~IST40XX_SINGLETAP;

		break;
	case 1:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &data->client->dev, "%s: Set SingleTap Mode\n",
			   __func__);

		data->lpm_mode |= IST40XX_SINGLETAP;

		break;
	default:
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_info(true, &data->client->dev, "%s: Invalid Argument\n",
			   __func__);

		break;
	}

	if (data->status.sys_mode != STATE_POWER_OFF) {
		ret = ist40xx_write_sponge_reg(data, IST40XX_SPONGE_CTRL,
				(u16*)&data->lpm_mode, 1, true);
		if (ret) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			input_err(true, &data->client->dev,
				  "%s: fail to write sponge reg.\n", __func__);

			goto err;
		}

		ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
				(eHCOM_NOTIRY_G_REGMAP << 16) | IST40XX_ENABLE);
		if (ret) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			input_info(true, &data->client->dev,
				   "%s: fail to write notify packet.\n", __func__);

			goto err;
		}
	}

err:
	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buf, sizeof(buf), "OK");
	else
		snprintf(buf, sizeof(buf), "NG");

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void fod_enable(void *dev_data)
{
	int ret;
	char buf[16] = { 0 };
	u8 fod_property;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (!data->dt_data->support_fod) {
		input_err(true, &data->client->dev, "%s not supported\n", __func__);
		snprintf(buf, sizeof(buf), "%s", "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	fod_property = !!sec->cmd_param[1];

	switch (sec->cmd_param[0]) {
	case 0:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &data->client->dev, "%s: Unset FOD Mode\n",
			   __func__);

		data->lpm_mode &= ~IST40XX_FOD;
		data->fod_property = fod_property;

		ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			(eHCOM_SET_FOD_DISABLE << 16) | data->fod_property);
		if (ret) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			input_err(true, &data->client->dev,
					"%s: fail to write fod disable.\n", __func__);
			goto err;
		}

		break;
	case 1:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &data->client->dev, "%s: Set FOD Mode\n",
			   __func__);

		data->lpm_mode |= IST40XX_FOD;
		data->fod_property = fod_property;

		ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			(eHCOM_SET_FOD_ENABLE << 16) | data->fod_property);
		if (ret) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			input_err(true, &data->client->dev,
					"%s: fail to write fod enable.\n", __func__);
			goto err;
		}

		break;
	default:
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_info(true, &data->client->dev, "%s: Invalid Argument\n",
			   __func__);

		break;
	}

err:
	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buf, sizeof(buf), "OK");
	else
		snprintf(buf, sizeof(buf), "NG");

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void get_threshold(void *dev_data)
{
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	snprintf(buf, sizeof(buf), "%d", tsp->threshold);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void get_scr_x_num(void *dev_data)
{
	int val = -1;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->tsp_info.dir.swap_xy)
		val = data->tsp_info.screen.tx;
	else
		val = data->tsp_info.screen.rx;

	if (val >= 0) {
		snprintf(buf, sizeof(buf), "%u", val);
		sec->cmd_state = SEC_CMD_STATUS_OK;

		input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
			   (int)strnlen(buf, sizeof(buf)));
	} else {
		snprintf(buf, sizeof(buf), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		input_err(true, &data->client->dev, "%s: fail to read num of x (%d).\n",
			   __func__, val);
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
}

static void get_scr_y_num(void *dev_data)
{
	int val = -1;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->tsp_info.dir.swap_xy)
		val = data->tsp_info.screen.rx;
	else
		val = data->tsp_info.screen.tx;

	if (val >= 0) {
		snprintf(buf, sizeof(buf), "%u", val);
		sec->cmd_state = SEC_CMD_STATUS_OK;

		input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
			   (int)strnlen(buf, sizeof(buf)));
	} else {
		snprintf(buf, sizeof(buf), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		input_err(true, &data->client->dev, "%s: fail to read num of y (%d).\n",
			   __func__, val);
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
}

static void get_all_x_num(void *dev_data)
{
	int val = -1;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->tsp_info.dir.swap_xy)
		val = data->tsp_info.ch_num.tx;
	else
		val = data->tsp_info.ch_num.rx;

	if (val >= 0) {
		snprintf(buf, sizeof(buf), "%u", val);
		sec->cmd_state = SEC_CMD_STATUS_OK;

		input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
			   (int)strnlen(buf, sizeof(buf)));
	} else {
		snprintf(buf, sizeof(buf), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		input_err(true, &data->client->dev, "%s: fail to read num of x (%d).\n",
			   __func__, val);
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
}

static void get_all_y_num(void *dev_data)
{
	int val = -1;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->tsp_info.dir.swap_xy)
		val = data->tsp_info.ch_num.rx;
	else
		val = data->tsp_info.ch_num.tx;

	if (val >= 0) {
		snprintf(buf, sizeof(buf), "%u", val);
		sec->cmd_state = SEC_CMD_STATUS_OK;

		input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
			   (int)strnlen(buf, sizeof(buf)));
	} else {
		snprintf(buf, sizeof(buf), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		input_err(true, &data->client->dev, "%s: fail to read num of y (%d).\n",
			  __func__, val);
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
}

int check_tsp_channel(struct ist40xx_data *data, int width, int height)
{
	int node = -EPERM;
	struct sec_cmd_data *sec = &data->sec;

	if (data->tsp_info.dir.swap_xy) {
		if ((sec->cmd_param[0] < 0) || (sec->cmd_param[0] >= height) ||
			(sec->cmd_param[1] < 0) || (sec->cmd_param[1] >= width)) {
			input_err(true, &data->client->dev,
				  "%s: parameter error: %u,%u\n", __func__,
				  sec->cmd_param[0], sec->cmd_param[1]);
		} else {
			node = sec->cmd_param[1] + sec->cmd_param[0] * width;
			input_info(true, &data->client->dev, "%s: node = %d\n",
				   __func__, node);
		}
	} else {
		if ((sec->cmd_param[0] < 0) || (sec->cmd_param[0] >= width) ||
			(sec->cmd_param[1] < 0) || (sec->cmd_param[1] >= height)) {
			input_err(true, &data->client->dev,
				  "%s: parameter error: %u,%u\n", __func__,
				  sec->cmd_param[0], sec->cmd_param[1]);
		} else {
			node = sec->cmd_param[0] + sec->cmd_param[1] * width;
			input_info(true, &data->client->dev, "%s: node = %d\n",
				   __func__, node);
		}
	}

	return node;
}

static u16 cal_cp_value[IST40XX_MAX_NODE_NUM];
static u16 cal_self_cp_value[IST40XX_MAX_SELF_NODE_NUM];
static u16 cal_prox_cp_value[IST40XX_MAX_SELF_NODE_NUM];

void get_cp_array(void *dev_data)
{
	int i, ret;
	int count = 0;
	char *buf;
	const int msg_len = 16;
	char msg[msg_len];
	char buff[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		goto out;
	}

	mutex_lock(&data->lock);

	ret = ist40xx_read_cp_node(data, &tsp->node, true);
	if (ret) {
		mutex_unlock(&data->lock);
		input_err(true, &data->client->dev, "%s: tsp cp read fail!\n",
			  __func__);

		goto out;
	}

	mutex_unlock(&data->lock);

	ist40xx_parse_cp_node(data, &tsp->node, true);

	ret = parse_cp_node(data, &tsp->node, cal_cp_value, cal_self_cp_value,
			cal_prox_cp_value, true);
	if (ret) {
		input_err(true, &data->client->dev, "%s: tsp cp parse fail!\n",
			  __func__);
		goto out;
	}

	buf = kmalloc(IST40XX_MAX_NODE_NUM * 5, GFP_KERNEL);
	if (!buf) {
		input_err(true, &data->client->dev, "%s: couldn't allocate memory\n",
			  __func__);
		goto out;
	}

	memset(buf, 0, IST40XX_MAX_NODE_NUM * 5);

	for (i = 0; i < tsp->ch_num.rx * tsp->ch_num.tx; i++) {
		count += snprintf(msg, msg_len, "%d,", cal_cp_value[i]);
		strncat(buf, msg, msg_len);
	}

	sec_cmd_set_cmd_result(sec, buf, count - 1);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf, count - 1);

	kfree(buf);

	return;

out:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void get_self_cp_array(void *dev_data)
{
	int i, ret;
	int count = 0;
	char *buf;
	const int msg_len = 16;
	char msg[msg_len];
	char buff[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			   "%s: now sys_mode status is not STATE_POWER_ON!\n",
			   __func__);
		goto out;
	}

	mutex_lock(&data->lock);

	ret = ist40xx_read_cp_node(data, &tsp->node, true);
	if (ret) {
		mutex_unlock(&data->lock);
		input_err(true, &data->client->dev, "%s: tsp self cp read fail!\n",
			  __func__);

		goto out;
	}

	mutex_unlock(&data->lock);

	ist40xx_parse_cp_node(data, &tsp->node, true);

	ret = parse_cp_node(data, &tsp->node, cal_cp_value, cal_self_cp_value,
			cal_prox_cp_value, true);
	if (ret) {
		input_err(true, &data->client->dev, "%s: tsp self cp parse fail!\n",
			  __func__);
		goto out;
	}

	buf = kmalloc(IST40XX_MAX_SELF_NODE_NUM * 5, GFP_KERNEL);
	if (!buf) {
		input_err(true, &data->client->dev, "%s: couldn't allocate memory\n",
			  __func__);
		goto out;
	}

	memset(buf, 0, IST40XX_MAX_SELF_NODE_NUM * 5);

	for (i = 0; i < tsp->ch_num.rx + tsp->ch_num.tx; i++) {
		count += snprintf(msg, msg_len, "%d,", cal_self_cp_value[i]);
		strncat(buf, msg, msg_len);
	}

	sec_cmd_set_cmd_result(sec, buf, count - 1);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf, count - 1);

	kfree(buf);

	return;

out:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void get_prox_cp_array(void *dev_data)
{
	int i, ret;
	int count = 0;
	char *buf;
	const int msg_len = 16;
	char msg[msg_len];
	char buff[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			   "%s: now sys_mode status is not STATE_POWER_ON!\n",
			   __func__);
		goto out;
	}

	mutex_lock(&data->lock);
	ret = ist40xx_read_cp_node(data, &tsp->node, true);
	if (ret) {
		mutex_unlock(&data->lock);
		input_err(true, &data->client->dev, "%s: tsp prox cp read fail!\n",
			  __func__);

		goto out;
	}

	mutex_unlock(&data->lock);

	ist40xx_parse_cp_node(data, &tsp->node, true);

	ret = parse_cp_node(data, &tsp->node, cal_cp_value, cal_self_cp_value,
			cal_prox_cp_value, true);
	if (ret) {
		input_err(true, &data->client->dev, "%s: tsp prox cp parse fail!\n",
			  __func__);

		goto out;
	}

	buf = kmalloc(IST40XX_MAX_SELF_NODE_NUM * 5, GFP_KERNEL);
	if (!buf) {
		input_err(true, &data->client->dev, "%s: couldn't allocate memory\n",
			  __func__);

		goto out;
	}

	memset(buf, 0, IST40XX_MAX_SELF_NODE_NUM * 5);

	for (i = 0; i < tsp->ch_num.rx + tsp->ch_num.tx; i++) {
		count += snprintf(msg, msg_len, "%d,", cal_prox_cp_value[i]);
		strncat(buf, msg, msg_len);
	}

	sec_cmd_set_cmd_result(sec, buf, count - 1);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf, count - 1);

	kfree(buf);

	return;

out:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void run_miscalibration(void *dev_data)
{
	int ret = 0;
	int max_val = 0;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is STATE_POWER_OFF!\n",
			  __func__);
		goto out;
	}

	ret = ist40xx_miscalibrate(data);
	if (ret) {
		input_err(true, &data->client->dev, "%s: miscalibration fail!\n",
			  __func__);
		goto out;
	}

	max_val = data->status.miscalib_result;
	if (max_val > SEC_MISCAL_SPEC)
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	else
		sec->cmd_state = SEC_CMD_STATUS_OK;

	snprintf(buf, sizeof(buf), "0,%d", max_val);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)),
					   "MIS_CAL");
	}

	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));

	return;

out:
	snprintf(buf, sizeof(buf), "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)),
					   "MIS_CAL");
	}

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void run_miscalibration_all(void *dev_data)
{
	int i, ret = 0;
	int count = 0;
	char *buf;
	const int msg_len = 16;
	char msg[msg_len];
	char buff[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;
	struct TSP_NODE_BUF *node = &tsp->node;

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			"%s: now sys_mode status is STATE_POWER_OFF!\n",
			__func__);
		goto out;
	}

	ist40xx_delay(300);

	ret = ist40xx_miscalibrate(data);
	if (ret) {
		input_err(true, &data->client->dev, "%s: miscalibration fail!\n",
			__func__);
		goto out;
	}

	buf = kmalloc(IST40XX_MAX_NODE_NUM * 5, GFP_KERNEL);
	if (!buf) {
		input_err(true, &data->client->dev, "%s: couldn't allocate memory\n",
			__func__);
		goto out;
	}

	memset(buf, 0, IST40XX_MAX_NODE_NUM * 5);

	for (i = 0; i < tsp->ch_num.rx + tsp->ch_num.tx; i++) {
		count += snprintf(msg, msg_len, "%d,", node->miscal[i]);
		strncat(buf, msg, msg_len);
	}

	sec_cmd_set_cmd_result(sec, buf, count - 1);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf, count - 1);

	kfree(buf);

	return;

out:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;

}

void get_miscalibration_value(void *dev_data)
{
	int idx = 0;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;
	struct TSP_NODE_BUF *node = &tsp->node;

	sec_cmd_set_default_result(sec);

	idx = check_tsp_channel(data, tsp->screen.rx, tsp->screen.tx);
	if (idx < 0) {		// Parameter parsing fail
		snprintf(buf, sizeof(buf), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buf, sizeof(buf), "%d", node->miscal[idx]);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));
}

void get_mis_cal_info(void *dev_data)
{
	int ret = 0;
	int max_val = 0;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is STATE_POWER_OFF!\n",
			  __func__);
		goto out;
	}

	ret = ist40xx_miscalibrate(data);
	if (ret) {
		input_err(true, &data->client->dev, "%s: miscalibration fail!\n",
			  __func__);
		goto out;
	}

	max_val = data->status.miscalib_result;
	if (max_val > SEC_MISCAL_SPEC)
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	else
		sec->cmd_state = SEC_CMD_STATUS_OK;

	snprintf(buf, sizeof(buf), "0,%d", max_val);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)),
					   "MIS_CAL");
	}

	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));

	return;

out:
	snprintf(buf, sizeof(buf), "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)),
					   "MIS_CAL");
	}

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

static void check_connection(void *dev_data)
{
	int ret;
	u32 chk_value = 0;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	ret = ist40xx_read_cmd(data, rSYS_CHIPID, &chk_value);
	if (ret || (chk_value != data->chip_id)) {
		snprintf(buf, sizeof(buf), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buf, sizeof(buf), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void run_prox_intensity_read_all(void *dev_data)
{
	int ret = 0;
	u32 sensitivity = 0;
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	ret = ist40xx_read_reg(data->client, IST40XX_HIB_PROX_SENSITI,
			&sensitivity);
	if (ret) {
		input_info(true, &data->client->dev, "%s Failed get the touch status data\n", __func__);
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buf, sizeof(buf), "SUM_X:%d SUM_Y:%d THD_X:%d THD_Y:%d", (sensitivity >> 16) & 0xFFFF,
				sensitivity & 0xFFFF, tsp->prox_tx_threshold, tsp->prox_rx_threshold);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static u16 node_value[IST40XX_MAX_NODE_NUM];
static u16 self_node_value[IST40XX_MAX_SELF_NODE_NUM];
static u16 prox_node_value[IST40XX_MAX_SELF_NODE_NUM];

void get_cdc_array(void *dev_data)
{
	int i, ret;
	int count = 0;
	char *buf;
	const int msg_len = 16;
	char msg[msg_len];
	u8 flag = NODE_FLAG_CDC;
	char buff[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);

		goto out;
	}

	mutex_lock(&data->lock);

	ret = ist40xx_read_touch_node(data, flag, &tsp->node);
	if (ret) {
		mutex_unlock(&data->lock);
		input_err(true, &data->client->dev, "%s() tsp node read fail!\n", __func__);

		goto out;
	}

	mutex_unlock(&data->lock);

	ist40xx_parse_touch_node(data, &tsp->node);

	ret = parse_tsp_node(data, flag, &tsp->node, node_value, self_node_value,
			prox_node_value);
	if (ret) {
		input_err(true, &data->client->dev, "%s: tsp node parse fail!\n",
			  __func__);
		goto out;
	}

	buf = kmalloc(IST40XX_MAX_NODE_NUM * 5, GFP_KERNEL);
	if (!buf) {
		input_err(true, &data->client->dev, "%s: couldn't allocate memory\n",
			  __func__);
		goto out;
	}

	memset(buf, 0, IST40XX_MAX_NODE_NUM * 5);

	for (i = 0; i < tsp->ch_num.rx * tsp->ch_num.tx; i++) {
		count += snprintf(msg, msg_len, "%d,", node_value[i]);
		strncat(buf, msg, msg_len);
	}

	sec_cmd_set_cmd_result(sec, buf, count - 1);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf, count -1);

	kfree(buf);

	return;

out:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void get_self_cdc_array(void *dev_data)
{
	int i, ret;
	int count = 0;
	char *buf;
	const int msg_len = 16;
	char msg[msg_len];
	u8 flag = NODE_FLAG_CDC;
	char buff[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		goto out;
	}

	mutex_lock(&data->lock);

	ret = ist40xx_read_touch_node(data, flag, &tsp->node);
	if (ret) {
		mutex_unlock(&data->lock);
		input_err(true, &data->client->dev, "%s: tsp self cdc read fail!\n",
			  __func__);

		goto out;
	}

	mutex_unlock(&data->lock);

	ist40xx_parse_touch_node(data, &tsp->node);

	ret = parse_tsp_node(data, flag, &tsp->node, node_value, self_node_value,
			prox_node_value);
	if (ret) {
		input_err(true, &data->client->dev, "%s: tsp self cdc parse fail!\n",
			  __func__);
		goto out;
	}

	buf = kmalloc(IST40XX_MAX_SELF_NODE_NUM * 5, GFP_KERNEL);
	if (!buf) {
		input_err(true, &data->client->dev, "%s: couldn't allocate memory\n",
			  __func__);
		goto out;
	}

	memset(buf, 0, IST40XX_MAX_SELF_NODE_NUM * 5);

	for (i = 0; i < tsp->ch_num.rx + tsp->ch_num.tx; i++) {
		count += snprintf(msg, msg_len, "%d,", self_node_value[i]);
		strncat(buf, msg, msg_len);
	}

	sec_cmd_set_cmd_result(sec, buf, count - 1);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf, count - 1);

	kfree(buf);

	return;

out:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void get_prox_cdc_array(void *dev_data)
{
	int i, ret;
	int count = 0;
	char *buf;
	const int msg_len = 16;
	char msg[msg_len];
	u8 flag = NODE_FLAG_CDC;
	char buff[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		goto out;
	}

	mutex_lock(&data->lock);

	ret = ist40xx_read_touch_node(data, flag, &tsp->node);
	if (ret) {
		mutex_unlock(&data->lock);
		input_err(true, &data->client->dev, "%s: tsp prox cdc read fail!\n",
			  __func__);

		goto out;
	}

	mutex_unlock(&data->lock);

	ist40xx_parse_touch_node(data, &tsp->node);

	ret = parse_tsp_node(data, flag, &tsp->node, node_value, self_node_value,
			prox_node_value);
	if (ret) {
		input_err(true, &data->client->dev, "%s: tsp prox cdc parse fail!\n",
			  __func__);
		goto out;
	}

	buf = kmalloc(IST40XX_MAX_SELF_NODE_NUM * 5, GFP_KERNEL);
	if (!buf) {
		input_err(true, &data->client->dev, "%s: couldn't allocate memory\n",
			  __func__);
		goto out;
	}

	memset(buf, 0, IST40XX_MAX_SELF_NODE_NUM * 5);

	for (i = 0; i < tsp->ch_num.rx + tsp->ch_num.tx; i++) {
		count += snprintf(msg, msg_len, "%d,", prox_node_value[i]);
		strncat(buf, msg, msg_len);
	}

	sec_cmd_set_cmd_result(sec, buf, count - 1);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf, count - 1);

	kfree(buf);

	return;

out:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void run_cdc_read(void *dev_data)
{
	int i;
	int ret = 0;
	int min_val = 0, max_val = 0;
	char buf[16] = { 0 };
	u8 flag = NODE_FLAG_CDC;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		goto out;
	}

	ret = ist40xx_read_touch_node(data, flag, &tsp->node);
	if (ret) {
		input_err(true, &data->client->dev, "%s() tsp node read fail!\n",
			  __func__);
		goto out;
	}

	ist40xx_parse_touch_node(data, &tsp->node);

	ret = parse_tsp_node(data, flag, &tsp->node, node_value, self_node_value,
			prox_node_value);
	if (ret) {
		input_err(true, &data->client->dev, "%s: tsp node parse fail!\n",
			  __func__);
		goto out;
	}

	min_val = max_val = node_value[0];

	for (i = 0; i < tsp->screen.rx * tsp->screen.tx; i++) {
		max_val = max(max_val, (int)node_value[i]);
		min_val = min(min_val, (int)node_value[i]);
	}

	snprintf(buf, sizeof(buf), "%d,%d", min_val, max_val);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CR");

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));

	return;

out:
	snprintf(buf, sizeof(buf), "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CR");

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void run_cdc_read_all(void *dev_data)
{
	int i;
	int ret = 0;
	int count = 0;
	char *buf;
	const int msg_len = 16;
	char msg[msg_len];
	u8 flag = NODE_FLAG_CDC;
	char buff[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			"%s: now sys_mode status is not STATE_POWER_ON!\n",
			__func__);
		goto out;
	}

	ret = ist40xx_read_touch_node(data, flag, &tsp->node);
	if (ret) {
		input_err(true, &data->client->dev, "%s() tsp node read fail!\n",
			__func__);
		goto out;
	}

	ist40xx_parse_touch_node(data, &tsp->node);

	ret = parse_tsp_node(data, flag, &tsp->node, node_value, self_node_value,
			prox_node_value);
	if (ret) {
		input_err(true, &data->client->dev, "%s: tsp node parse fail!\n",
			__func__);
		goto out;
	}

	buf = kmalloc(IST40XX_MAX_NODE_NUM * 5, GFP_KERNEL);
	if (!buf) {
		input_err(true, &data->client->dev, "%s: couldn't allocate memory\n",
			__func__);
		goto out;
	}

	memset(buf, 0, IST40XX_MAX_NODE_NUM * 5);

	for (i = 0; i < tsp->screen.rx * tsp->screen.tx; i++) {
			count += snprintf(msg, msg_len, "%d,", node_value[i]);
			strncat(buf, msg, msg_len);
	}

	sec_cmd_set_cmd_result(sec, buf, count - 1);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf, count - 1);

	kfree(buf);

	return;

out:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void run_self_cdc_read(void *dev_data)
{
	int i;
	int ret = 0;
	int min_val, max_val;
	int min_val_tx, max_val_tx;
	int min_val_rx, max_val_rx;
	char buf[16] = { 0 };
	char buf_onecmd_1[16] = { 0 };
	char buf_onecmd_2[16] = { 0 };
	u8 flag = NODE_FLAG_CDC;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		goto out;
	}

	ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			(eHCOM_SET_TIME_ACTIVE << 16) | 30000);
	if (ret) {
		input_err(true, &data->client->dev, "%s: write active time fail!\n",
			  __func__);
		goto out;
	}

	ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			(eHCOM_SET_TIME_IDLE << 16) | 30000);
	if (ret) {
		input_err(true, &data->client->dev, "%s: write idle time fail!\n",
			  __func__);
		goto out;
	}

	ist40xx_delay(200);

	ret = ist40xx_read_touch_node(data, flag, &tsp->node);
	if (ret) {
		input_err(true, &data->client->dev, "%s: tsp node read fail!\n",
			  __func__);
		goto out;
	}

	ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			  (eHCOM_SET_TIME_ACTIVE << 16) | 0xFFFF);
	if (ret) {
		input_err(true, &data->client->dev, "%s: write active time fail!\n",
			  __func__);
		goto out;
	}

	ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			  (eHCOM_SET_TIME_IDLE << 16) | 0xFFFF);
	if (ret) {
		input_err(true, &data->client->dev, "%s: write idle time fail!\n",
			  __func__);
		goto out;
	}

	ist40xx_parse_touch_node(data, &tsp->node);

	ret = parse_tsp_node(data, flag, &tsp->node, node_value, self_node_value,
			prox_node_value);
	if (ret) {
		input_err(true, &data->client->dev, "%s: tsp node parse fail!\n",
			  __func__);
		goto out;
	}

	min_val = max_val = self_node_value[0];

	for (i = 0; i < tsp->ch_num.rx + tsp->ch_num.tx; i++) {
		max_val = max(max_val, (int)self_node_value[i]);
		min_val = min(min_val, (int)self_node_value[i]);
	}

	snprintf(buf, sizeof(buf), "%d,%d", min_val, max_val);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		min_val_tx = max_val_tx = self_node_value[0];

		for (i = 1; i < tsp->ch_num.tx; i++) {
			min_val_tx = min(min_val_tx, (int)self_node_value[i]);
			max_val_tx = max(max_val_tx, (int)self_node_value[i]);
		}

		min_val_rx = max_val_rx = self_node_value[tsp->ch_num.tx];

		for (i = 1; i < tsp->ch_num.rx; i++) {
			min_val_rx = min(min_val_rx,
					(int)self_node_value[i + tsp->ch_num.tx]);
			max_val_rx = max(max_val_rx,
					(int)self_node_value[i + tsp->ch_num.tx]);
		}

		snprintf(buf_onecmd_1, sizeof(buf_onecmd_1), "%d,%d", min_val_tx,
			 max_val_tx);
		snprintf(buf_onecmd_2, sizeof(buf_onecmd_2), "%d,%d", min_val_rx,
			 max_val_rx);

		sec_cmd_set_cmd_result_all(sec, buf_onecmd_1,
					   strnlen(buf_onecmd_1, sizeof(buf_onecmd_1)),
					   "SELF_CR_X");
		sec_cmd_set_cmd_result_all(sec, buf_onecmd_2,
					   strnlen(buf_onecmd_2, sizeof(buf_onecmd_2)),
					   "SELF_CR_Y");
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));

	return;

out:
	snprintf(buf, sizeof(buf), "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)),
					   "SELF_CR_X");
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)),
					   "SELF_CR_Y");
	}

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void run_self_cdc_read_all(void *dev_data)
{
	int i;
	int ret = 0;
	int count = 0;
	char *buf;
	const int msg_len = 16;
	char msg[msg_len];
	u8 flag = NODE_FLAG_CDC;
	char buff[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			"%s: now sys_mode status is not STATE_POWER_ON!\n",
			__func__);
		goto out;
	}

	ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			(eHCOM_SET_TIME_ACTIVE << 16) | 30000);
	if (ret) {
		input_err(true, &data->client->dev, "%s: write active time fail!\n",
			__func__);
		goto out;
	}

	ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			(eHCOM_SET_TIME_IDLE << 16) | 30000);
	if (ret) {
		input_err(true, &data->client->dev, "%s: write idle time fail!\n",
			__func__);
		goto out;
	}

	ist40xx_delay(200);

	ret = ist40xx_read_touch_node(data, flag, &tsp->node);
	if (ret) {
		input_err(true, &data->client->dev, "%s: tsp node read fail!\n",
			__func__);
		goto out;
	}

	ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			  (eHCOM_SET_TIME_ACTIVE << 16) | 0xFFFF);
	if (ret) {
		input_err(true, &data->client->dev, "%s: write active time fail!\n",
			__func__);
		goto out;
	}

	ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			  (eHCOM_SET_TIME_IDLE << 16) | 0xFFFF);
	if (ret) {
		input_err(true, &data->client->dev, "%s: write idle time fail!\n",
			__func__);
		goto out;
	}

	ist40xx_parse_touch_node(data, &tsp->node);

	ret = parse_tsp_node(data, flag, &tsp->node, node_value, self_node_value,
			prox_node_value);
	if (ret) {
		input_err(true, &data->client->dev, "%s: tsp node parse fail!\n",
			__func__);
		goto out;
	}

	buf = kmalloc(IST40XX_MAX_NODE_NUM * 5, GFP_KERNEL);
	if (!buf) {
		input_err(true, &data->client->dev, "%s: couldn't allocate memory\n",
			__func__);
		goto out;
	}

	memset(buf, 0, IST40XX_MAX_NODE_NUM * 5);

	for (i = 0; i < tsp->ch_num.tx + tsp->ch_num.rx; i++) {
			count += snprintf(msg, msg_len, "%d,", self_node_value[i]);
			strncat(buf, msg, msg_len);
	}

	sec_cmd_set_cmd_result(sec, buf, count - 1);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf, count - 1);

	kfree(buf);

	return;

out:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void run_prox_cdc_read(void *dev_data)
{
	int i;
	int ret = 0;
	int min_val, max_val;
	int min_val_tx, max_val_tx;
	int min_val_rx, max_val_rx;
	char buf[16] = { 0 };
	char buf_onecmd_1[16] = { 0 };
	char buf_onecmd_2[16] = { 0 };
	u8 flag = NODE_FLAG_CDC;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		goto out;
	}

	ret = ist40xx_read_touch_node(data, flag, &tsp->node);
	if (ret) {
		input_err(true, &data->client->dev, "%s: tsp node read fail!\n",
			  __func__);
		goto out;
	}

	ist40xx_parse_touch_node(data, &tsp->node);

	ret = parse_tsp_node(data, flag, &tsp->node, node_value, self_node_value,
			prox_node_value);
	if (ret) {
		input_err(true, &data->client->dev, "%s: tsp node parse fail!\n",
			  __func__);
		goto out;
	}

	min_val = max_val = prox_node_value[0];

	for (i = 0; i < tsp->ch_num.rx + tsp->ch_num.tx; i++) {
		max_val = max(max_val, (int)prox_node_value[i]);
		min_val = min(min_val, (int)prox_node_value[i]);
	}

	snprintf(buf, sizeof(buf), "%d,%d", min_val, max_val);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		min_val_tx = max_val_tx = prox_node_value[0];

		for (i = 1; i < tsp->ch_num.tx; i++) {
			min_val_tx = min(min_val_tx, (int)prox_node_value[i]);
			max_val_tx = max(max_val_tx, (int)prox_node_value[i]);
		}

		min_val_rx = max_val_rx = prox_node_value[tsp->ch_num.tx];

		for (i = 1; i < tsp->ch_num.rx; i++) {
			min_val_rx = min(min_val_rx,
					(int)prox_node_value[i + tsp->ch_num.tx]);
			max_val_rx = max(max_val_rx,
					(int)prox_node_value[i + tsp->ch_num.tx]);
		}

		snprintf(buf_onecmd_1, sizeof(buf_onecmd_1), "%d,%d", min_val_tx,
			 max_val_tx);
		snprintf(buf_onecmd_2, sizeof(buf_onecmd_2), "%d,%d", min_val_rx,
			 max_val_rx);
		sec_cmd_set_cmd_result_all(sec, buf_onecmd_1,
					   strnlen(buf_onecmd_1, sizeof(buf_onecmd_1)),
					   "PROXI_CR_X");
		sec_cmd_set_cmd_result_all(sec, buf_onecmd_2,
					   strnlen(buf_onecmd_2, sizeof(buf_onecmd_2)),
					   "PROXI_CR_Y");
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s() %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));

	return;

out:
	snprintf(buf, sizeof(buf), "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)),
					   "PROXI_CR_X");
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)),
					   "PROXI_CR_Y");
	}

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void get_cdc_value(void *dev_data)
{
	int idx = 0;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	idx = check_tsp_channel(data, tsp->screen.rx, tsp->screen.tx);
	if (idx < 0) {		// Parameter parsing fail
		snprintf(buf, sizeof(buf), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buf, sizeof(buf), "%d", node_value[idx]);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));
}

void get_self_cdc_value(void *dev_data)
{
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if ((sec->cmd_param[0] < 0) ||
		(sec->cmd_param[0] >= (tsp->ch_num.rx + tsp->ch_num.tx))) {
		snprintf(buf, sizeof(buf), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buf, sizeof(buf), "%d", self_node_value[sec->cmd_param[0]]);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));
}

void get_prox_cdc_value(void *dev_data)
{
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if ((sec->cmd_param[0] < 0) ||
		(sec->cmd_param[0] >= (tsp->ch_num.rx + tsp->ch_num.tx))) {
		snprintf(buf, sizeof(buf), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buf, sizeof(buf), "%d", prox_node_value[sec->cmd_param[0]]);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));
}

void get_rx_self_cdc_value(void *dev_data)
{
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if ((sec->cmd_param[1] < 0) || (sec->cmd_param[1] >= (tsp->ch_num.rx))) {
		snprintf(buf, sizeof(buf), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buf, sizeof(buf), "%d",
			 self_node_value[sec->cmd_param[1] + tsp->ch_num.tx]);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));
}

void get_tx_self_cdc_value(void *dev_data)
{
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if ((sec->cmd_param[0] < 0) || (sec->cmd_param[0] >= (tsp->ch_num.tx))) {
		snprintf(buf, sizeof(buf), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buf, sizeof(buf), "%d", self_node_value[sec->cmd_param[0]]);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));
}

void get_rx_prox_cdc_value(void *dev_data)
{
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if ((sec->cmd_param[1] < 0) || (sec->cmd_param[1] >= (tsp->ch_num.rx))) {
		snprintf(buf, sizeof(buf), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buf, sizeof(buf), "%d",
			 prox_node_value[sec->cmd_param[1] + tsp->ch_num.tx]);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));
}

void get_tx_prox_cdc_value(void *dev_data)
{
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if ((sec->cmd_param[0] < 0) || (sec->cmd_param[0] >= (tsp->ch_num.tx))) {
		snprintf(buf, sizeof(buf), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buf, sizeof(buf), "%d", prox_node_value[sec->cmd_param[0]]);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));
}

void run_cs_raw_read_all(void *dev_data)
{
	int i, j, idx, ret;
	int count = 0;
	char *buf;
	const int msg_len = 16;
	char msg[msg_len];
	u8 flag = NODE_FLAG_CDC;
	char buff[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		goto out;
	}

	mutex_lock(&data->lock);

	ret = ist40xx_read_touch_node(data, flag, &tsp->node);
	if (ret) {
		mutex_unlock(&data->lock);
		input_err(true, &data->client->dev, "%s: tsp node read fail!\n",
			  __func__);

		goto out;
	}

	mutex_unlock(&data->lock);

	ist40xx_parse_touch_node(data, &tsp->node);

	ret = parse_tsp_node(data, flag, &tsp->node, node_value, self_node_value,
			prox_node_value);
	if (ret) {
		input_err(true, &data->client->dev, "%s: tsp node parse fail!\n",
			  __func__);
		goto out;
	}

	buf = kmalloc(IST40XX_MAX_NODE_NUM * 5, GFP_KERNEL);
	if (!buf) {
		input_err(true, &data->client->dev, "%s: couldn't allocate memory\n",
			  __func__);
		goto out;
	}

	memset(buf, 0, IST40XX_MAX_NODE_NUM * 5);

	for (i = 0; i < tsp->ch_num.tx; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			idx = i * tsp->ch_num.rx + j;
			count += snprintf(msg, msg_len, "%d,", node_value[idx]);
			strncat(buf, msg, msg_len);
		}
	}

	sec_cmd_set_cmd_result(sec, buf, count - 1);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf, count - 1);

	kfree(buf);

	return;

out:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void run_cs_delta_read_all(void *dev_data)
{
	int i, j, idx, ret;
	int count = 0;
	char *buf;
	const int msg_len = 16;
	char msg[msg_len];
	u8 flag = NODE_FLAG_CDC;
	char buff[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		goto out;
	}

	mutex_lock(&data->lock);
	ret = ist40xx_read_touch_node(data, flag, &tsp->node);
	if (ret) {
		mutex_unlock(&data->lock);
		input_err(true, &data->client->dev, "%s: tsp node read fail!\n",
			  __func__);

		goto out;
	}

	mutex_unlock(&data->lock);

	ist40xx_parse_touch_node(data, &tsp->node);

	ret = parse_tsp_node(data, flag, &tsp->node, node_value, self_node_value,
			prox_node_value);
	if (ret) {
		input_err(true, &data->client->dev, "%s: tsp node parse fail!\n",
			  __func__);
		goto out;
	}

	buf = kmalloc(IST40XX_MAX_NODE_NUM * 5, GFP_KERNEL);
	if (!buf) {
		input_err(true, &data->client->dev, "%s: couldn't allocate memory\n",
			  __func__);
		goto out;
	}

	memset(buf, 0, IST40XX_MAX_NODE_NUM * 5);

	for (i = 0; i < tsp->ch_num.tx; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			idx = i * tsp->ch_num.rx + j;
			count += snprintf(msg, msg_len, "%d,",
					node_value[idx] - tsp->baseline);
			strncat(buf, msg, msg_len);
		}
	}

	sec_cmd_set_cmd_result(sec, buf, count - 1);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf, count - 1);

	kfree(buf);

	return;

out:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

#define MAX_DCM_DEFAULT		500
void get_max_dcm(void *dev_data)
{
	int ret = 0;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	u32 max_dcm = 0;

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		goto out;
	}

	ret = ist40xx_read_cmd(data, eHCOM_GET_MAX_DCM, &max_dcm);
	if (ret) {
		ist40xx_reset(data, false);
		ist40xx_start(data);
		max_dcm = 0;
	}

out:
	if (max_dcm == 0)
		max_dcm = MAX_DCM_DEFAULT;

	snprintf(buf, sizeof(buf), "%d", max_dcm);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));
}

extern u8 *ts_cmcs_bin;
extern u32 ts_cmcs_bin_size;
extern CMCS_BUF *cmcs_buf;
static int tx_cm_gap_value[IST40XX_MAX_NODE_NUM];
static int rx_cm_gap_value[IST40XX_MAX_NODE_NUM];

int get_read_all_data(struct ist40xx_data *data, u8 flag)
{
	int ii;
	int count = 0;
	int type;
	char *buffer;
	char *temp;
	int ret;
	u32 cmcs_flag = 0;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)&data->sec;
	TSP_INFO *tsp = &data->tsp_info;

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		goto out;
	}

	if (flag == TEST_CM_ALL_DATA)
		cmcs_flag = CMCS_FLAG_CM;
	else if ((flag == TEST_SLOPE0_ALL_DATA) || (flag == TEST_SLOPE1_ALL_DATA))
		cmcs_flag = CMCS_FLAG_CM | CMCS_FLAG_SLOPE;
	else if (flag == TEST_CS_ALL_DATA)
		cmcs_flag = CMCS_FLAG_CS;

	mutex_lock(&data->lock);

	ret = ist40xx_cmcs_test(data, cmcs_flag);
	if (ret) {
		mutex_unlock(&data->lock);
		input_err(true, &data->client->dev, "%s: tsp cmcs test fail!\n",
			  __func__);

		goto out;
	}

	mutex_unlock(&data->lock);

	buffer = kzalloc(IST40XX_MAX_NODE_NUM * 5, GFP_KERNEL);
	if (!buffer) {
		input_err(true, &data->client->dev, "%s: failed to buffer alloc\n",
			  __func__);
		goto out;
	}

	temp = kzalloc(10, GFP_KERNEL);
	if (!temp) {
		input_err(true, &data->client->dev, "%s: failed to temp alloc\n",
			  __func__);
		goto alloc_out;
	}

	for (ii = 0; ii < tsp->ch_num.rx * tsp->ch_num.tx; ii++) {
		type = check_tsp_type(data, ii / tsp->ch_num.rx, ii % tsp->ch_num.rx);
		if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED)) {
			count += snprintf(temp, 10, "%d,", 0);
		} else {
			switch (flag) {
			case TEST_CM_ALL_DATA:
				count += snprintf(temp, 10, "%d,", cmcs_buf->cm[ii]);
				break;
			case TEST_SLOPE0_ALL_DATA:
				count += snprintf(temp, 10, "%d,", cmcs_buf->slope0[ii]);
				break;
			case TEST_SLOPE1_ALL_DATA:
				count += snprintf(temp, 10, "%d,", cmcs_buf->slope1[ii]);
				break;
			case TEST_CS_ALL_DATA:
				count += snprintf(temp, 10, "%d,", cmcs_buf->cs[ii]);
				break;
			}
		}

		strncat(buffer, temp, 10);
	}

	sec_cmd_set_cmd_result(sec, buffer, count - 1);
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buffer,
		   strnlen(buffer, sizeof(buffer)) - 1);

	kfree(buffer);
	kfree(temp);

	return 0;

alloc_out:
	kfree(buffer);
out:
	snprintf(buf, sizeof(buf), "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	return -1;
}

void get_cm_all_data(void *dev_data)
{
	int ret;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	ret = get_read_all_data(data, TEST_CM_ALL_DATA);
	if (ret < 0)
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	else
		sec->cmd_state = SEC_CMD_STATUS_OK;
}

void run_cm_read_all(void *dev_data)
{
	int ret;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			"%s: now sys_mode status is not STATE_POWER_ON!\n",
			__func__);
		goto out;
	}

	ret = ist40xx_get_cmcs_info(ts_cmcs_bin, ts_cmcs_bin_size);
	if (ret) {
		input_err(true, &data->client->dev, "%s: get cmcs info read fail!\n",
			__func__);
		goto out;
	}

	ret = get_read_all_data(data, TEST_CM_ALL_DATA);
	if (ret < 0)
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	else
		sec->cmd_state = SEC_CMD_STATUS_OK;

	return;

out:
	snprintf(buf, sizeof(buf), "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void get_slope0_all_data(void *dev_data)
{
	int ret;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	ret = get_read_all_data(data, TEST_SLOPE0_ALL_DATA);
	if (ret < 0)
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	else
		sec->cmd_state = SEC_CMD_STATUS_OK;
}

void get_slope1_all_data(void *dev_data)
{
	int ret;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	ret = get_read_all_data(data, TEST_SLOPE1_ALL_DATA);
	if (ret < 0)
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	else
		sec->cmd_state = SEC_CMD_STATUS_OK;
}

void get_cs_all_data(void *dev_data)
{
	int ret;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	ret = get_read_all_data(data, TEST_CS_ALL_DATA);
	if (ret < 0)
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	else
		sec->cmd_state = SEC_CMD_STATUS_OK;
}

void run_cm_test(void *dev_data)
{
	int i, j;
	int ret = 0;
	char buf[16] = { 0 };
	int type, idx;
	int next_idx;
	int last_col;
	int min_val, max_val;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		goto out;
	}

	ret = ist40xx_get_cmcs_info(ts_cmcs_bin, ts_cmcs_bin_size);
	if (ret) {
		input_err(true, &data->client->dev, "%s: get cmcs info read fail!\n",
			  __func__);
		goto out;
	}

	mutex_lock(&data->lock);

	ret = ist40xx_cmcs_test(data, CMCS_FLAG_CM);
	if (ret) {
		mutex_unlock(&data->lock);
		input_err(true, &data->client->dev, "%s: tsp cmcs test fail!\n",
			  __func__);

		goto out;
	}

	mutex_unlock(&data->lock);

	/* tx gap = abs(x-y)/x * 100   with x=Rx0,Tx0, y=Rx0,Tx1, ...*/
	for (i = 0; i < tsp->ch_num.tx - 1; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			idx = i * tsp->ch_num.rx + j;
			next_idx = idx + tsp->ch_num.rx;

			if (cmcs_buf->cm[idx])
				tx_cm_gap_value[idx] = 100 *
					(s16)abs(cmcs_buf->cm[idx] - cmcs_buf->cm[next_idx]) /
					cmcs_buf->cm[idx];
			else
				tx_cm_gap_value[idx] = 9999; /* the value is out of spec */
		}
	}
	/*rx gap = abs(x-y)/x * 100   with x=Rx0,Tx0, y=Rx1,Tx0, ... */
	last_col = tsp->ch_num.rx - 1;
	for (i = 0; i < tsp->ch_num.tx; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			idx = i * tsp->ch_num.rx + j;
			if ((idx % tsp->ch_num.rx) == last_col)
				continue;

			next_idx = idx + 1;

			if (cmcs_buf->cm[idx])
				rx_cm_gap_value[idx] = 100 *
					(s16)abs(cmcs_buf->cm[idx] - cmcs_buf->cm[next_idx]) /
					cmcs_buf->cm[idx];
			else
				rx_cm_gap_value[idx] = 9999;
		}
	}

	min_val = max_val = cmcs_buf->cm[0];
	for (i = 0; i < tsp->ch_num.tx; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			idx = (i * tsp->ch_num.rx) + j;

			type = check_tsp_type(data, i, j);
			if (type == TSP_CH_USED) {
				max_val = max(max_val, (int)cmcs_buf->cm[idx]);
				min_val = min(min_val, (int)cmcs_buf->cm[idx]);
			}
		}
	}

	snprintf(buf, sizeof(buf), "%d,%d,%d", min_val, max_val, max_val - min_val);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CM");

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));

	return;

out:
	snprintf(buf, sizeof(buf), "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CM");

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void get_cm_value(void *dev_data)
{
	int idx = 0;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	idx = check_tsp_channel(data, tsp->ch_num.rx, tsp->ch_num.tx);
	if (idx < 0) {		// Parameter parsing fail
		snprintf(buf, sizeof(buf), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buf, sizeof(buf), "%d", cmcs_buf->cm[idx]);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));
}

void get_cm_maxgap_value(void *dev_data)
{
	int i, j;
	int idx;
	int last_col;
	int max_cm_gap_tx;
	int max_cm_gap_rx;
	char buf[16] = { 0 };
	char buf_onecmd[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	max_cm_gap_tx = tx_cm_gap_value[0];

	for (i = 0; i < tsp->ch_num.tx - 1; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			idx = i * tsp->ch_num.rx + j;
			max_cm_gap_tx = max(max_cm_gap_tx, tx_cm_gap_value[idx]);
		}
	}

	max_cm_gap_rx = rx_cm_gap_value[0];
	last_col = tsp->ch_num.rx - 1;

	for (i = 0; i < tsp->ch_num.tx; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			idx = i * tsp->ch_num.rx + j;
			if ((idx % tsp->ch_num.rx) == last_col)
				continue;

			max_cm_gap_rx = max(max_cm_gap_rx, rx_cm_gap_value[idx]);
		}
	}

	snprintf(buf, sizeof(buf), "%d,%d", max_cm_gap_tx, max_cm_gap_rx);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		snprintf(buf_onecmd, sizeof(buf_onecmd), "0,%d",
			max(max_cm_gap_rx, max_cm_gap_tx));
		sec_cmd_set_cmd_result_all(sec, buf_onecmd,
					   strnlen(buf_onecmd, sizeof(buf_onecmd)),
					   "CM_GAP");
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));
}

#define CMD_RESULT_WORD_LEN	10
void get_cm_maxgap_all(void *dev_data)
{
	char temp[CMD_RESULT_WORD_LEN] = { 0 };
	char *buf = NULL;
	int total_node, ii;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	total_node = tsp->ch_num.rx * tsp->ch_num.tx;
	buf = kzalloc(total_node * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buf)
		return;

	for (ii = 0; ii < total_node; ii++) {
		snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", max(tx_cm_gap_value[ii],
					rx_cm_gap_value[ii]));
		strncat(buf, temp, CMD_RESULT_WORD_LEN);
		memset(temp, 0x00, CMD_RESULT_WORD_LEN);
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf,
				total_node * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buf);
}

void get_tx_cm_gap_value(void *dev_data)
{
	int idx = 0;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	idx = check_tsp_channel(data, tsp->ch_num.rx, tsp->ch_num.tx);
	if (idx < 0) {		// Parameter parsing fail
		snprintf(buf, sizeof(buf), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buf, sizeof(buf), "%d", tx_cm_gap_value[idx]);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));
}

void get_rx_cm_gap_value(void *dev_data)
{
	int idx = 0;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	idx = check_tsp_channel(data, tsp->ch_num.rx, tsp->ch_num.tx);
	if (idx < 0) {		// Parameter parsing fail
		snprintf(buf, sizeof(buf), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buf, sizeof(buf), "%d", rx_cm_gap_value[idx]);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));
}

void run_cmjit_test(void *dev_data)
{
	int i, j;
	int ret = 0;
	char buf[16] = { 0 };
	int type, idx;
	int min_val, max_val;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		goto out;
	}

	ret = ist40xx_get_cmcs_info(ts_cmcs_bin, ts_cmcs_bin_size);
	if (ret) {
		input_err(true, &data->client->dev, "%s: get cmcs info read fail!\n",
			  __func__);
		goto out;
	}

	mutex_lock(&data->lock);

	ret = ist40xx_cmcs_test(data, CMCS_FLAG_CMJIT);
	if (ret) {
		mutex_unlock(&data->lock);
		input_err(true, &data->client->dev, "%s: tsp cmcs test fail!\n",
			  __func__);

		goto out;
	}

	mutex_unlock(&data->lock);

	min_val = max_val = cmcs_buf->cm_jit[0];

	for (i = 0; i < tsp->ch_num.tx; i++) {
		for (j = 0; j < tsp->ch_num.rx; j++) {
			idx = (i * tsp->ch_num.rx) + j;
			type = check_tsp_type(data, i, j);

			if (type == TSP_CH_USED) {
				max_val = max(max_val, (int)cmcs_buf->cm_jit[idx]);
				min_val = min(min_val, (int)cmcs_buf->cm_jit[idx]);
			}
		}
	}

	snprintf(buf, sizeof(buf), "%d,%d", min_val, max_val);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));

	return;

out:
	snprintf(buf, sizeof(buf), "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void get_cmjit_value(void *dev_data)
{
	int idx = 0;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	idx = check_tsp_channel(data, tsp->ch_num.rx, tsp->ch_num.tx);
	if (idx < 0) {		// Parameter parsing fail
		snprintf(buf, sizeof(buf), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buf, sizeof(buf), "%d", cmcs_buf->cm_jit[idx]);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));
}

void run_cmcs_test(void *dev_data)
{
	int ret = 0;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		goto out;
	}

	ret = ist40xx_get_cmcs_info(ts_cmcs_bin, ts_cmcs_bin_size);
	if (ret) {
		input_err(true, &data->client->dev, "%s: get cmcs info read fail!\n",
			  __func__);
		goto out;
	}

	mutex_lock(&data->lock);

	ret = ist40xx_cmcs_test(data, CMCS_FLAG_CM);
	if (ret) {
		mutex_unlock(&data->lock);
		input_err(true, &data->client->dev, "%s: tsp cmcs test fail!\n",
			  __func__);

		goto out;
	}

	mutex_unlock(&data->lock);

	snprintf(buf, sizeof(buf), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));

	return;

out:
	snprintf(buf, sizeof(buf), "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void get_cm_array(void *dev_data)
{
	int i;
	int count = 0;
	int type;
	char *buf;
	const int msg_len = 16;
	char msg[msg_len];
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	buf = kmalloc(IST40XX_MAX_NODE_NUM * 5, GFP_KERNEL);
	if (!buf) {
		input_err(true, &data->client->dev,
			  "%s: couldn't allocate memory\n", __func__);
		goto out;
	}

	memset(buf, 0, IST40XX_MAX_NODE_NUM * 5);

	for (i = 0; i < tsp->ch_num.rx * tsp->ch_num.tx; i++) {
		type = check_tsp_type(data, i / tsp->ch_num.rx, i % tsp->ch_num.rx);
		if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED))
			count += snprintf(msg, msg_len, "%d,", 0);
		else
			count += snprintf(msg, msg_len, "%d,", cmcs_buf->cm[i]);

		strncat(buf, msg, msg_len);
	}

	sec_cmd_set_cmd_result(sec, buf, count - 1);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf, count);

	kfree(buf);

	return;

out:
	snprintf(buf, sizeof(buf), "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void get_slope0_array(void *dev_data)
{
	int i;
	int count = 0;
	char *buf;
	const int msg_len = 16;
	char msg[msg_len];
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	buf = kmalloc(IST40XX_MAX_NODE_NUM * 5, GFP_KERNEL);
	if (!buf) {
		input_err(true, &data->client->dev, "%s: couldn't allocate memory\n",
			  __func__);
		goto out;
	}

	memset(buf, 0, IST40XX_MAX_NODE_NUM * 5);

	for (i = 0; i < tsp->ch_num.rx * tsp->ch_num.tx; i++) {
		count += snprintf(msg, msg_len, "%d,", cmcs_buf->slope0[i]);
		strncat(buf, msg, msg_len);
	}

	sec_cmd_set_cmd_result(sec, buf, count - 1);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf, count - 1);

	kfree(buf);

	return;

out:
	snprintf(buf, sizeof(buf), "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void get_slope1_array(void *dev_data)
{
	int i;
	int count = 0;
	char *buf;
	const int msg_len = 16;
	char msg[msg_len];
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	buf = kmalloc(IST40XX_MAX_NODE_NUM * 5, GFP_KERNEL);
	if (!buf) {
		input_err(true, &data->client->dev, "%s: couldn't allocate memory\n",
			  __func__);
		goto out;
	}

	memset(buf, 0, IST40XX_MAX_NODE_NUM * 5);

	for (i = 0; i < tsp->ch_num.rx * tsp->ch_num.tx; i++) {
		count += snprintf(msg, msg_len, "%d,", cmcs_buf->slope1[i]);
		strncat(buf, msg, msg_len);
	}

	sec_cmd_set_cmd_result(sec, buf, count - 1);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf, count - 1);

	kfree(buf);

	return;

out:
	snprintf(buf, sizeof(buf), "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void get_cs_array(void *dev_data)
{
	int i;
	int count = 0;
	int type;
	char *buf;
	const int msg_len = 16;
	char msg[msg_len];
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	TSP_INFO *tsp = &data->tsp_info;

	sec_cmd_set_default_result(sec);

	buf = kmalloc(IST40XX_MAX_NODE_NUM * 5, GFP_KERNEL);
	if (!buf) {
		input_err(true, &data->client->dev, "%s: couldn't allocate memory\n",
			  __func__);
		goto out;
	}

	memset(buf, 0, IST40XX_MAX_NODE_NUM * 5);

	for (i = 0; i < tsp->ch_num.rx * tsp->ch_num.tx; i++) {
		type = check_tsp_type(data, i / tsp->ch_num.rx, i % tsp->ch_num.rx);
		if ((type == TSP_CH_UNKNOWN) || (type == TSP_CH_UNUSED))
			count += snprintf(msg, msg_len, "%d,", 0);
		else
			count += snprintf(msg, msg_len, "%d,", cmcs_buf->cs[i]);

		strncat(buf, msg, msg_len);
	}

	sec_cmd_set_cmd_result(sec, buf, count - 1);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf, count - 1);

	kfree(buf);

	return;

out:
	snprintf(buf, sizeof(buf), "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void check_fail_channel(struct ist40xx_data *data, u32 *tx_result,
		u32 *rx_result, char *buf)
{
	int i;
	char msg[128] = { 0 };
	char fail_buf[256] = { 0 };
	int count = 0;
	int fail_cnt = 0;
	TSP_INFO *tsp = &data->tsp_info;

	count = sprintf(msg, "(");
	strcat(fail_buf, msg);

	for (i = 0; i < tsp->ch_num.tx; i++) {
		if (fail_cnt >= 5)
			break;

		if (i < 32) {
			if ((tx_result[0] >> i) & 0x1) {
				count += sprintf(msg, "TX%d,", i);
				strcat(fail_buf, msg);
				fail_cnt++;
			}
		} else {
			if ((tx_result[1] >> (i - 32)) & 0x1) {
				count += sprintf(msg, "TX%d,", i);
				strcat(fail_buf, msg);
				fail_cnt++;
			}
		}
	}

	for (i = 0; i < tsp->ch_num.rx; i++) {
		if (fail_cnt >= 5)
			break;

		if (i < 32) {
			if ((rx_result[0] >> i) & 0x1) {
				count += sprintf(msg, "RX%d,", i);
				strcat(fail_buf, msg);
				fail_cnt++;
			}
		} else {
			if ((rx_result[1] >> (i - 32)) & 0x1) {
				count += sprintf(msg, "RX%d,", i);
				strcat(fail_buf, msg);
				fail_cnt++;
			}
		}
	}

	strncat(buf, fail_buf, strlen(fail_buf) - 1);

	sprintf(msg, ")");
	strcat(buf, msg);
}

void run_cmcs_full_test(void *dev_data)
{
	int ret = 0;
	bool cm_result, cs_result, micro_cm_result, old_cmd = 0;
	char msg[128] = { 0 };
	char fail_msg[256] = { 0 };
	char buf[256] = { 0 };
	char test[32];
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev, "%s: couldn't allocate memory\n",
				__func__);
		snprintf(buf, sizeof(buf), "NG");
		goto out;
	}

	snprintf(test, sizeof(test), "%d", sec->cmd_param[0]);

	memset(msg, 0, sizeof(msg));
	
	if (sec->cmd_param[0] == 3) {
		/* 3 : bridge short, micro short */
		sprintf(msg, "NA");
		strcat(buf, msg);
		goto na;
	}

	if (sec->cmd_param[0] == 1 && sec->cmd_param[1] == 1) {
		ret = ist40xx_get_cmcs_info(ts_cmcs_bin, ts_cmcs_bin_size);
		if (ret) {
			input_err(true, &data->client->dev, "%s: get cmcs info read fail!\n",
				__func__);
			snprintf(buf, sizeof(buf), "NG");
			goto out;
		}

		mutex_lock(&data->lock);

		ret = ist40xx_cmcs_test(data,
				CMCS_FLAG_CM | CMCS_FLAG_SLOPE | CMCS_FLAG_CS);
		if (ret) {
			mutex_unlock(&data->lock);
			input_err(true, &data->client->dev, "%s: tsp cmcs test fail!\n",
				__func__);
			snprintf(buf, sizeof(buf), "NG");
			goto out;
		}

		mutex_unlock(&data->lock);
	}

	/* CM result */
	if (cmcs_buf->cm_result == 1) {
		cm_result = 1;
		input_err(true, &data->client->dev, "CM result: fail\n");
	} else {
		cm_result = 0;
		input_err(true, &data->client->dev, "CM result: pass\n");
	}

	/* CS result */
	if (cmcs_buf->cs_result == 1) {
		cs_result = 1;
		input_err(true, &data->client->dev, "CS result: fail\n");
	} else {
		cs_result = 0;
		input_err(true, &data->client->dev, "CS result: pass\n");
	}

	/* Micro cm result */
	if ((cmcs_buf->cm_result == 0) && (cmcs_buf->slope_result == 1)) {
		micro_cm_result = 1;
		input_err(true, &data->client->dev, "Micro CM result: fail\n");
	} else {
		micro_cm_result = 0;
		input_err(true, &data->client->dev, "Micro CM result: pass\n");
	}

	memset(fail_msg, 0, sizeof(fail_msg));

	if (sec->cmd_param[0] == 1 && sec->cmd_param[1] == 1) {
		/* 1,1 : open */
		if (cm_result) {
			check_fail_channel(data, cmcs_buf->cm_tx_result,
				cmcs_buf->cm_rx_result, fail_msg);
			sprintf(msg, "OPEN:%s", fail_msg);
			strcat(buf, msg);
			goto out;
		} else {
			sprintf(msg, "OPEN:OK");
			strcat(buf, msg);
		}
	} else if (sec->cmd_param[0] == 1 && sec->cmd_param[1] == 2) {
		/* 1,2 : short */
		if (cs_result) {
			check_fail_channel(data, cmcs_buf->cs_tx_result,
				cmcs_buf->cs_rx_result, fail_msg);
			sprintf(msg, "SHORT:%s", fail_msg);
			strcat(buf, msg);
			goto out;
		} else {
			sprintf(msg, "SHORT:OK");
			strcat(buf, msg);
		}
	} else if (sec->cmd_param[0] == 2) {
		/* 2 : micro open, crack */
		if (micro_cm_result) {
			check_fail_channel(data, cmcs_buf->cm_slope_result,
				cmcs_buf->cm_slope_result, fail_msg);
			sprintf(msg, "CRACK:%s", fail_msg);
			strcat(buf, msg);
			goto out;
		} else {
			sprintf(msg, "CRACK:OK");
			strcat(buf, msg);
		}
	} else if (sec->cmd_param[0] == 1 && sec->cmd_param[1] == 0) {
		/* 1,0 : old command, return CONT */
		sprintf(msg, "CONT");
		strcat(buf, msg);
	} else {
		/* 0 or else : old command */
		old_cmd = true;
		if (cm_result) {
			check_fail_channel(data, cmcs_buf->cm_tx_result,
				cmcs_buf->cm_rx_result, fail_msg);
			sprintf(msg, "FAIL%s;", fail_msg);
		} else {
			sprintf(msg, "PASS;");
		}
		strcat(buf, msg);

		memset(fail_msg, 0, sizeof(fail_msg));
		if (cs_result) {
			check_fail_channel(data, cmcs_buf->cs_tx_result,
				cmcs_buf->cs_rx_result, fail_msg);
			sprintf(msg, "FAIL%s;", fail_msg);
		} else {
			sprintf(msg, "PASS;");
		}
		strcat(buf, msg);

		memset(fail_msg, 0, sizeof(fail_msg));
		if (micro_cm_result) {
			check_fail_channel(data, cmcs_buf->slope_tx_result,
				cmcs_buf->slope_rx_result, fail_msg);
			sprintf(msg, "FAIL;%s", fail_msg);
		} else {
			sprintf(msg, "PASS;");
		}
		strcat(buf, msg);

		sprintf(msg, "NA");
		strcat(buf, msg);


		if (strnstr(msg, "T", sizeof(msg)))
			data->ito_test[3] |= 0x10;
		if (strnstr(msg, "R", sizeof(msg)))
			data->ito_test[3] |= 0x20;
		if (cs_result)
			data->ito_test[3] |= 0x0F;
	}

na:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;
	if (sec->cmd_param[0] == 3) {
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	}

	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
			(int)strnlen(buf, sizeof(buf)));

	if (old_cmd ||cm_result || cs_result || micro_cm_result)
		sec_cmd_send_event_to_user(sec, test, "RESULT=FAIL");
	else
		sec_cmd_send_event_to_user(sec, test, "RESULT=PASS");

	return;

out:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
			(int)strnlen(buf, sizeof(buf)));

	sec_cmd_send_event_to_user(sec, test, "RESULT=FAIL");
}

int ist40xx_execute_force_calibration(struct i2c_client *client, int cal_mode)
{
	struct ist40xx_data *data = i2c_get_clientdata(client);
	int ret = -ENOEXEC;
	int wait_cnt = 1;

	input_info(true, &data->client->dev, "*** Force Auto Calibrate %ds ***\n",
		   CALIB_WAIT_TIME / 10);

	data->status.calib = 1;
	ist40xx_disable_irq(data);
	while (1) {
		ret = ist40xx_cmd_calibrate(data);
		if (ret)
			continue;

		ist40xx_enable_irq(data);
		ret = ist40xx_calib_wait(data);
		if (!ret)
			break;

		ist40xx_disable_irq(data);

		if (--wait_cnt == 0)
			break;

		ist40xx_reset(data, false);
	}

	ist40xx_disable_irq(data);
	ist40xx_reset(data, false);
	data->status.calib = 0;
	ist40xx_enable_irq(data);

	return ret;
}

void run_force_calibration(void *dev_data)
{
	int ret = 0;
	char buf[16] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		goto out;
	}

	if (data->touch_pressed_num != 0) {
		input_err(true, &data->client->dev, "%s: return (finger cnt %d)\n",
			   __func__, data->touch_pressed_num);

		goto out;
	}

	mutex_lock(&data->lock);
	ist40xx_reset(data, false);

	ret = ist40xx_execute_force_calibration(data->client, 0);
	if (ret) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buf, sizeof(buf), "%s", "NG");
	} else {
#ifdef TCLM_CONCEPT
		/* devide tclm case */
		sec_tclm_case(data->tdata, sec->cmd_param[0]);

		input_info(true, &data->client->dev, "%s: param, %d, %c, %d\n",
			   __func__, sec->cmd_param[0], sec->cmd_param[0],
			   data->tdata->root_of_calibration);

		ret = sec_execute_tclm_package(data->tdata, 1);
		if (ret < 0) {
			input_info(true, &data->client->dev,
				   "%s: sec_execute_tclm_package\n", __func__);
		}

		sec_tclm_root_of_cal(data->tdata, CALPOSITION_NONE);
#endif

		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buf, sizeof(buf), "%s", "OK");
	}

	ist40xx_start(data);

	mutex_unlock(&data->lock);

	data->tdata->external_factory = false;

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
	return;

out:
	snprintf(buf, sizeof(buf), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

void get_force_calibration(void *dev_data)
{
	int ret = 0;
	char buf[16] = { 0 };
	u32 calib_msg;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		snprintf(buf, sizeof(buf), "NG");

		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);

		goto err;
	}

	ret = ist40xx_read_cmd(data, eHCOM_GET_CAL_RESULT_S, &calib_msg);
	if (ret) {
		mutex_lock(&data->lock);
		ist40xx_reset(data, false);
		ist40xx_start(data);
		mutex_unlock(&data->lock);

		snprintf(buf, sizeof(buf), "%s", "NG");

		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_err(true, &data->client->dev, "Error Read SLF Calibration Result\n");

		goto err;
	}

	if (((calib_msg & CALIB_MSG_MASK) != CALIB_MSG_VALID) ||
		(CALIB_TO_STATUS(calib_msg) != 0)) {
		snprintf(buf, sizeof(buf), "%s", "NG");

		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		goto err;
	}

	ret = ist40xx_read_cmd(data, eHCOM_GET_CAL_RESULT_M, &calib_msg);
	if (ret) {
		mutex_lock(&data->lock);
		ist40xx_reset(data, false);
		ist40xx_start(data);
		mutex_unlock(&data->lock);

		snprintf(buf, sizeof(buf), "%s", "NG");

		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_err(true, &data->client->dev, "Error Read MTL Calibration Result\n");

		goto err;
	}

	if (((calib_msg & CALIB_MSG_MASK) != CALIB_MSG_VALID) ||
		(CALIB_TO_STATUS(calib_msg) != 0)) {
		snprintf(buf, sizeof(buf), "NG");

		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		goto err;
	}

	ret = ist40xx_read_cmd(data, eHCOM_GET_CAL_RESULT_P, &calib_msg);
	if (ret) {
		mutex_lock(&data->lock);
		ist40xx_reset(data, false);
		ist40xx_start(data);
		mutex_unlock(&data->lock);

		snprintf(buf, sizeof(buf), "NG");

		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_err(true, &data->client->dev, "Error Read PROX Calibration Result\n");

		goto err;
	}

	if (((calib_msg & CALIB_MSG_MASK) != CALIB_MSG_VALID) ||
		(CALIB_TO_STATUS(calib_msg) != 0)) {
		snprintf(buf, sizeof(buf), "NG");

		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		goto err;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
err:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

#ifdef TCLM_CONCEPT
static void set_external_factory(void *dev_data)
{
	char buf[22] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	data->tdata->external_factory = true;

	sec_cmd_set_default_result(sec);

	snprintf(buf, sizeof(buf), "OK");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s\n", __func__, buf);
}

int ist40xx_tclm_data_read(struct i2c_client *client, int address)
{
	int i, ret = 0;
	u32 fw_ver = 0;
	u32 nbuff[IST40XX_NVM_OFFSET_LENGTH];
	struct ist40xx_data *data = i2c_get_clientdata(client);

	switch (address) {
	case SEC_TCLM_NVM_OFFSET_IC_FIRMWARE_VER:
		fw_ver = ist40xx_get_fw_ver(data);
		/*need check 16 or 32 bits*/
		return (fw_ver & 0xFFFF);

	case SEC_TCLM_NVM_ALL_DATA:
		ret = ist40xx_read_sec_info(data, IST40XX_NVM_OFFSET_FAC_RESULT, nbuff,
					    IST40XX_NVM_OFFSET_LENGTH);
		if (ret) {
			input_err(true, &data->client->dev,
				  "%s: [ERROR] read_tsp_nvm_data ret:%d\n",
				  __func__, ret);
			return -1;
		}

		data->test_result.data[0] =
			(u8)(nbuff[IST40XX_NVM_OFFSET_FAC_RESULT] & 0xFF);
		data->disassemble_count = nbuff[IST40XX_NVM_OFFSET_DISASSEMBLE_COUNT];
		data->tdata->nvdata.cal_count =
			(u8)(nbuff[IST40XX_NVM_OFFSET_CAL_COUNT] & 0xFF);
		data->tdata->nvdata.tune_fix_ver =
			(u16)(nbuff[IST40XX_NVM_OFFSET_TUNE_VERSION] & 0xFFFF);
		data->tdata->nvdata.cal_position =
			(u8)(nbuff[IST40XX_NVM_OFFSET_CAL_POSITION] & 0xFF);
		data->tdata->nvdata.cal_pos_hist_cnt =
			(u8)(nbuff[IST40XX_NVM_OFFSET_HISTORY_QUEUE_COUNT] & 0xFF);
		data->tdata->nvdata.cal_pos_hist_lastp =
			(u8)(nbuff[IST40XX_NVM_OFFSET_HISTORY_QUEUE_LASTP] & 0xFF);

		for (i = IST40XX_NVM_OFFSET_HISTORY_QUEUE_ZERO; i < IST40XX_NVM_OFFSET_LENGTH; i++) {
			data->tdata->nvdata.cal_pos_hist_queue[2 * (i - IST40XX_NVM_OFFSET_HISTORY_QUEUE_ZERO)] = (u8)((nbuff[i] & 0xFFFF) >> 8);
			data->tdata->nvdata.cal_pos_hist_queue[2 * (i - IST40XX_NVM_OFFSET_HISTORY_QUEUE_ZERO) + 1] = (u8)(nbuff[i] & 0xFF);
		}

		input_info(true, &data->client->dev, "%s: %d %X %x %d %d\n",
			   __func__, data->tdata->nvdata.cal_count,
			   data->tdata->nvdata.tune_fix_ver,
			   data->tdata->nvdata.cal_position,
			   data->tdata->nvdata.cal_pos_hist_cnt,
			   data->tdata->nvdata.cal_pos_hist_lastp);

		return ret;
	default:
		return ret;
	}
}

int ist40xx_tclm_data_write(struct i2c_client *client, int address)
{
	int i, ret = 1;
	u32 nbuff[IST40XX_NVM_OFFSET_LENGTH];
	struct ist40xx_data *data = i2c_get_clientdata(client);

	input_info(true, &data->client->dev,
		   "%s: write SEC_TCLM_NVM_ALL_DATA: %d %X %x %d %d\n", __func__,
		   data->tdata->nvdata.cal_count, data->tdata->nvdata.tune_fix_ver,
		   data->tdata->nvdata.cal_position,
		   data->tdata->nvdata.cal_pos_hist_cnt,
		   data->tdata->nvdata.cal_pos_hist_lastp);

	memset(nbuff, 0x00, IST40XX_NVM_OFFSET_LENGTH);

	nbuff[IST40XX_NVM_OFFSET_FAC_RESULT] = (u32)data->test_result.data[0];
	nbuff[IST40XX_NVM_OFFSET_DISASSEMBLE_COUNT] = data->disassemble_count;
	nbuff[IST40XX_NVM_OFFSET_CAL_COUNT] = (u32)data->tdata->nvdata.cal_count;
	nbuff[IST40XX_NVM_OFFSET_TUNE_VERSION] =
		(u32)data->tdata->nvdata.tune_fix_ver;
	nbuff[IST40XX_NVM_OFFSET_CAL_POSITION] =
		(u32)data->tdata->nvdata.cal_position;
	nbuff[IST40XX_NVM_OFFSET_HISTORY_QUEUE_COUNT] =
		(u32)data->tdata->nvdata.cal_pos_hist_cnt;
	nbuff[IST40XX_NVM_OFFSET_HISTORY_QUEUE_LASTP] =
		(u32)data->tdata->nvdata.cal_pos_hist_lastp;

		for (i = IST40XX_NVM_OFFSET_HISTORY_QUEUE_ZERO; i < IST40XX_NVM_OFFSET_LENGTH; i++) {
			nbuff[i] = (u32)((data->tdata->nvdata.cal_pos_hist_queue[2 * (i - IST40XX_NVM_OFFSET_HISTORY_QUEUE_ZERO)] << 8) |
					 (data->tdata->nvdata.cal_pos_hist_queue[2 * (i - IST40XX_NVM_OFFSET_HISTORY_QUEUE_ZERO) + 1]));
	}

	ret = ist40xx_write_sec_info(data, IST40XX_NVM_OFFSET_FAC_RESULT, nbuff,
				     IST40XX_NVM_OFFSET_LENGTH);
	if (ret) {
		input_err(true, &data->client->dev, "%s: [ERROR] set_tsp_nvm_data ret:%d\n",
			  __func__, ret);
		return -1;
	}

	return ret;
}

void get_pat_information(void *dev_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	char buf[50] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buf, sizeof(buf), "C%02XT%04X.%4s%s%c%d%c%d%c%d",
		data->tdata->nvdata.cal_count, data->tdata->nvdata.tune_fix_ver,
		data->tdata->tclm_string[data->tdata->nvdata.cal_position].f_name,
		(data->tdata->tclm_level == TCLM_LEVEL_LOCKDOWN) ? ".L " : " ",
		data->tdata->cal_pos_hist_last3[0], data->tdata->cal_pos_hist_last3[1],
		data->tdata->cal_pos_hist_last3[2], data->tdata->cal_pos_hist_last3[3],
		data->tdata->cal_pos_hist_last3[4], data->tdata->cal_pos_hist_last3[5]);

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   strnlen(buf, sizeof(buf)));
}

/* FACTORY TEST RESULT SAVING FUNCTION
* bit 3 ~ 0 : OCTA Assy
* bit 7 ~ 4 : OCTA module
* param[0] : OCTA module(1) / OCTA Assy(2)
* param[1] : TEST NONE(0) / TEST FAIL(1) / TEST PASS(2) : 2 bit
*/
static void get_tsp_test_result(void *dev_data)
{
	u32 *buf32;
	int ret;
	int len = 1;		//1~32
	char buf[50] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		goto out;
	}

	if (ist40xx_intr_wait(data, 30) < 0) {
		input_err(true, &data->client->dev,"%s: intr wait fail", __func__);
		goto out;
	}

	buf32 = kzalloc(len * sizeof(u32), GFP_KERNEL);
	if (!buf32) {
		input_err(true, &data->client->dev, "%s: failed to allocate\n",
			  __func__);
		goto out;
	}

	ret = ist40xx_read_sec_info(data, IST40XX_NVM_OFFSET_FAC_RESULT, buf32,
			len);
	if (ret) {
		kfree(buf32);
		input_err(true, &data->client->dev, "%s: sec info read fail\n",
			  __func__);

		goto out;
	}

	input_info(true, &data->client->dev, "%s: [%2d]:%08X\n", __func__, len,
		  buf32[0]);

	data->test_result.data[0] = buf32[0] & 0xff;
	kfree(buf32);

	input_info(true, &data->client->dev, "%s: %X", __func__,
		  data->test_result.data[0]);

	if (data->test_result.data[0] == 0xFF) {
		input_info(true, &data->client->dev,
			   "%s: clear factory_result as zero\n", __func__);
		data->test_result.data[0] = 0;
	}

	snprintf(buf, sizeof(buf), "M:%s, M:%d, A:%s, A:%d",
		 data->test_result.module_result == 0 ? "NONE" :
		 data->test_result.module_result == 1 ? "FAIL" : "PASS",
		 data->test_result.module_count,
		 data->test_result.assy_result == 0 ? "NONE" :
		 data->test_result.assy_result == 1 ? "FAIL" : "PASS",
		 data->test_result.assy_count);

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s() %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));

	return;

out:
	snprintf(buf, sizeof(buf), "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

static void set_tsp_test_result(void *dev_data)
{
	int ret;
	char buf[16] = { 0 };
	u32 *buf32;
	u32 temp = 0;
	int len = 1;		//1~32
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode != STATE_POWER_ON) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		goto out;
	}

	if (ist40xx_intr_wait(data, 30) < 0) {
		input_err(true, &data->client->dev, "%s: intr wait fail", __func__);
		goto out;
	}

	buf32 = kzalloc(len * sizeof(u32), GFP_KERNEL);
	if (!buf32) {
		input_err(true, &data->client->dev, "%s: failed to allocate\n",
			  __func__);
		goto out;
	}

	ret = ist40xx_read_sec_info(data, IST40XX_NVM_OFFSET_FAC_RESULT, buf32,
			len);
	if (ret) {
		kfree(buf32);
		input_err(true, &data->client->dev, "%s: sec info read fail\n",
			  __func__);

		goto out;
	}

	input_info(true, &data->client->dev, "%s: [%2d]:%08X\n", __func__, len,
		   buf32[0]);

	data->test_result.data[0] = buf32[0] & 0xff;
	kfree(buf32);

	input_info(true, &data->client->dev, "%s: %X", __func__,
		   data->test_result.data[0]);

	if (data->test_result.data[0] == 0xFF) {
		input_info(true, &data->client->dev, "%s: clear factory_result as zero\n",
			   __func__);
		data->test_result.data[0] = 0;
	}

	if (sec->cmd_param[0] == TEST_OCTA_ASSAY) {
		data->test_result.assy_result = sec->cmd_param[1];
		if (data->test_result.assy_count < 3)
			data->test_result.assy_count++;
	} else if (sec->cmd_param[0] == TEST_OCTA_MODULE) {
		data->test_result.module_result = sec->cmd_param[1];
		if (data->test_result.module_count < 3)
			data->test_result.module_count++;
	}

	input_info(true, &data->client->dev, "%s: [0x%X] M:%s, M:%d, A:%s, A:%d\n",
		   __func__, data->test_result.data[0],
		   data->test_result.module_result == 0 ? "NONE" :
		   data->test_result.module_result == 1 ? "FAIL" : "PASS",
		   data->test_result.module_count,
		   data->test_result.assy_result == 0 ? "NONE" :
		   data->test_result.assy_result == 1 ? "FAIL" : "PASS",
		   data->test_result.assy_count);

	temp = data->test_result.data[0];
	ist40xx_write_sec_info(data, IST40XX_NVM_OFFSET_FAC_RESULT, &temp, 1);

	snprintf(buf, sizeof(buf), "OK");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s() %s(%d)\n", __func__, buf,
			(int)strnlen(buf, sizeof(buf)));

	return;

out:
	snprintf(buf, sizeof(buf), "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
void ist40xx_read_sec_info_all(void *dev_data)
{
	int ret = 0;
	char buf[16] = { 0 };
	u32 *buf32;
	int len = 32;		//4~32 (0~32)
	int i;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode == STATE_POWER_OFF) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		goto out;
	}

	if (ist40xx_intr_wait(data, 30) < 0) {
		input_err(true, &data->client->dev, "%s: intr wait fail", __func__);
		goto out;
	}

	buf32 = kzalloc(len * sizeof(u32), GFP_KERNEL);
	if (!buf32) {
		input_err(true, &data->client->dev, "%s: failed to allocate\n",
			  __func__);
		goto out;
	}

	ret = ist40xx_read_sec_info(data, 0, buf32, len);
	if (ret) {
		kfree(buf32);
		input_err(true, &data->client->dev, "%s: sec info read fail\n",
			  __func__);

		goto out;
	}

	for (i = (len - 1); i > 0; i -= 4) {
		input_info(true, &data->client->dev, "%s: [%2d]:%08X %08X %08X %08X\n",
			  __func__, i, buf32[i], buf32[i - 1], buf32[i - 2], buf32[i - 3]);
	}

	kfree(buf32);

	snprintf(buf, sizeof(buf), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));

	return;

out:
	snprintf(buf, sizeof(buf), "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}
#endif

static void increase_disassemble_count(void *dev_data)
{
	int ret;
	char buf[16] = { 0 };
	u32 buf32[1] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode == STATE_POWER_OFF) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		goto out;
	}

	ist40xx_read_sec_info(data, IST40XX_NVM_OFFSET_DISASSEMBLE_COUNT, buf32, 1);

	input_info(true, &data->client->dev, "%s: disassemble count is #1 %d\n",
		   __func__, buf32[0]);

	if ((buf32[0] & 0xff) == 0xFF)
		buf32[0] = 0;

	if (buf32[0] < 0xFE)
		buf32[0]++;

	data->disassemble_count = buf32[0];

	ret = ist40xx_write_sec_info(data, IST40XX_NVM_OFFSET_DISASSEMBLE_COUNT,
				     buf32, 1);
	if (ret < 0) {
		input_err(true, &data->client->dev, "%s: nvm write failed. ret: %d\n",
			  __func__, ret);
		goto out;
	}

	ist40xx_delay(20);

	ist40xx_read_sec_info(data, IST40XX_NVM_OFFSET_DISASSEMBLE_COUNT, buf32, 1);
	input_info(true, &data->client->dev, "%s: check disassemble count: %d\n",
		   __func__, buf32[0]);

	snprintf(buf, sizeof(buf), "OK");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));

	return;

out:
	snprintf(buf, sizeof(buf), "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

static void get_disassemble_count(void *dev_data)
{
	u32 buf32;
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode == STATE_POWER_OFF) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		goto out;
	}

	memset(buf, 0x00, SEC_CMD_STR_LEN);

	ist40xx_read_sec_info(data, IST40XX_NVM_OFFSET_DISASSEMBLE_COUNT, &buf32,
			1);
	if ((buf32 & 0xff) == 0xFF) {
		buf32 = 0;
		ist40xx_write_sec_info(data, IST40XX_NVM_OFFSET_DISASSEMBLE_COUNT,
				&buf32, 1);
	}

	data->disassemble_count = buf32;

	input_info(true, &data->client->dev, "%s: read disassemble count: %d\n",
		   __func__, buf32 & 0xff);

	snprintf(buf, sizeof(buf), "%d", buf32 & 0xff);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));

	return;

out:
	snprintf(buf, sizeof(buf), "NG");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}
#endif

static void check_ic_mode(void *dev_data)
{
	int ret = 0;
	char buf[16] = { 0 };
	u32 status = 0;
	u8 mode = TOUCH_STATUS_NORMAL_MODE;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode == STATE_POWER_ON) {
		ret = ist40xx_read_reg(data->client, IST40XX_HIB_TOUCH_STATUS, &status);
		if (ret) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			input_err(true, &data->client->dev,
				  "%s: Failed get the touch status data\n", __func__);
		} else {
			if ((status & TOUCH_STATUS_MASK) == TOUCH_STATUS_MAGIC) {
				if (GET_NOISE_MODE(status))
					mode |= TOUCH_STATUS_NOISE_MODE;

				if (GET_WET_MODE(status))
					mode |= TOUCH_STATUS_WET_MODE;

				sec->cmd_state = SEC_CMD_STATUS_OK;
			} else {
				sec->cmd_state = SEC_CMD_STATUS_FAIL;
				input_err(true, &data->client->dev,
					  "%s: invalid touch status \n", __func__);
			}
		}
	} else {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
	}

	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buf, sizeof(buf), "%d", mode);
	else
		snprintf(buf, sizeof(buf), "NG");

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void get_wet_mode(void *dev_data)
{
	int ret = 0;
	char buf[16] = { 0 };
	u32 status = 0;
	u8 mode = TOUCH_STATUS_NORMAL_MODE;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode == STATE_POWER_ON) {
		ret = ist40xx_read_reg(data->client, IST40XX_HIB_TOUCH_STATUS,
				&status);
		if (ret) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			input_err(true, &data->client->dev,
				  "%s: Failed get the touch status data \n", __func__);
		} else {
			if ((status & TOUCH_STATUS_MASK) == TOUCH_STATUS_MAGIC) {
				if (GET_WET_MODE(status))
					mode = true;

				sec->cmd_state = SEC_CMD_STATUS_OK;
			} else {
				sec->cmd_state = SEC_CMD_STATUS_FAIL;
				input_err(true, &data->client->dev,
					  "%s() invalid touch status \n", __func__);
			}
		}
	} else {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
	}

	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buf, sizeof(buf), "%d", mode);
	else
		snprintf(buf, sizeof(buf), "NG");

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)),
				"WET_MODE");

	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

static void run_fw_integrity(void *dev_data)
{
	int ret = 0;
	char buf[16] = { 0 };
	u32 integrity = 0;
	u32 chksum = 0;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->status.sys_mode == STATE_POWER_ON) {
		ist40xx_reset(data, false);

		ret = ist40xx_read_cmd(data, eHCOM_GET_FW_INTEGRITY, &integrity);
		if (ret) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			input_err(true, &data->client->dev,
				  "%s: Failed get the fw integrity \n", __func__);
		} else {
			if (integrity == IST40XX_EXCEPT_INTEGRITY) {
				chksum = ist40xx_get_fw_chksum(data);
				if (chksum == 0) {
					sec->cmd_state = SEC_CMD_STATUS_FAIL;
					input_err(true, &data->client->dev,
						  "%s: Failed get the checksum data\n",
						  __func__);
				} else {
					sec->cmd_state = SEC_CMD_STATUS_OK;
				}
			} else {
				sec->cmd_state = SEC_CMD_STATUS_FAIL;
				input_err(true, &data->client->dev,
					  "%s: FW is broken \n", __func__);
			}
		}

		ist40xx_start(data);
	} else {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
	}

	if (sec->cmd_state == SEC_CMD_STATUS_OK) {
		snprintf(buf, sizeof(buf), "OK");
		input_err(true, &data->client->dev,
			  "%s: chksum %08x\n", __func__, chksum);
	} else
		snprintf(buf, sizeof(buf), "NG");

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_info(true, &data->client->dev, "%s: %s(%d)\n", __func__, buf,
		   (int)strnlen(buf, sizeof(buf)));
}

/* sysfs: /sys/class/sec/tsp/close_tsp_test */
static ssize_t show_close_tsp_test(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return snprintf(buf, FACTORY_BUF_SIZE, "%u\n", 0);
}

static void factory_cmd_result_all(void *dev_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	sec->item_count = 0;
	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	if (data->status.sys_mode != STATE_POWER_ON) {
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);

		goto out;
	}

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	get_chip_vendor(sec);
	get_chip_name(sec);
	get_fw_ver_bin(sec);
	get_fw_ver_ic(sec);

	ist40xx_delay(300);		/* need Stabilize time after reset(cal) */

	run_cdc_read(sec);		/*run_reference_read*/
	run_self_cdc_read(sec);		/*run_self_reference_read*/
	run_prox_cdc_read(sec);
	get_wet_mode(sec);	
	run_cm_test(sec);
	get_cm_maxgap_value(sec);
	run_miscalibration(sec);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;
out:
	input_err(true, &data->client->dev, "%s: %d%s\n", __func__, sec->item_count,
		  sec->cmd_result_all);
}

struct sec_cmd sec_cmds[] = {
	{SEC_CMD("get_chip_vendor", get_chip_vendor),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("get_chip_id", get_chip_id),},
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_threshold", get_threshold),},
	{SEC_CMD("get_checksum_data", get_checksum_data),},
	{SEC_CMD("get_x_num", get_scr_x_num),},
	{SEC_CMD("get_y_num", get_scr_y_num),},
	{SEC_CMD("get_all_x_num", get_all_x_num),},
	{SEC_CMD("get_all_y_num", get_all_y_num),},
	{SEC_CMD("dead_zone_enable", dead_zone_enable),},
	{SEC_CMD("clear_cover_mode", not_support_cmd),},
	{SEC_CMD("call_mode", call_mode),},
	{SEC_CMD("glove_mode", glove_mode),},
	{SEC_CMD("set_grip_data", set_grip_data),},
	{SEC_CMD("set_touchable_area", set_touchable_area),},
	{SEC_CMD("hover_enable", not_support_cmd),},
	{SEC_CMD("spay_enable", spay_enable),},
	{SEC_CMD("aot_enable", aot_enable),},
	{SEC_CMD("aod_enable", aod_enable),},
	{SEC_CMD("set_aod_rect", set_aod_rect),},
	{SEC_CMD("get_aod_rect", get_aod_rect),},
	{SEC_CMD("singletap_enable", singletap_enable),},
	{SEC_CMD("fod_enable", fod_enable),},
	{SEC_CMD("get_cp_array", get_cp_array),},
	{SEC_CMD("get_self_cp_array", get_self_cp_array),},
	{SEC_CMD("get_prox_cp_array", get_prox_cp_array),},
	{SEC_CMD("run_reference_read", run_cdc_read),},
	{SEC_CMD("get_reference", get_cdc_value),},
	{SEC_CMD("run_self_reference_read", run_self_cdc_read),},
	{SEC_CMD("run_prox_reference_read", run_prox_cdc_read),},
	{SEC_CMD("get_self_reference", get_self_cdc_value),},
	{SEC_CMD("get_prox_reference", get_prox_cdc_value),},
	{SEC_CMD("get_rx_self_reference", get_rx_self_cdc_value),},
	{SEC_CMD("get_tx_self_reference", get_tx_self_cdc_value),},
	{SEC_CMD("get_rx_prox_reference", get_rx_prox_cdc_value),},
	{SEC_CMD("get_tx_prox_reference", get_tx_prox_cdc_value),},
	{SEC_CMD("run_cdc_read", run_cdc_read),},
	{SEC_CMD("run_cdc_read_all", run_cdc_read_all),},
	{SEC_CMD("run_self_cdc_read", run_self_cdc_read),},
	{SEC_CMD("run_self_cdc_read_all", run_self_cdc_read_all),},
	{SEC_CMD("run_prox_cdc_read", run_prox_cdc_read),},
	{SEC_CMD("get_cdc_value", get_cdc_value),},
	{SEC_CMD("get_self_cdc_value", get_self_cdc_value),},
	{SEC_CMD("get_prox_cdc_value", get_prox_cdc_value),},
	{SEC_CMD("get_cdc_all_data", get_cdc_array),},
	{SEC_CMD("get_self_cdc_all_data", get_self_cdc_array),},
	{SEC_CMD("get_prox_cdc_all_data", get_prox_cdc_array),},
	{SEC_CMD("get_cdc_array", get_cdc_array),},
	{SEC_CMD("get_self_cdc_array", get_self_cdc_array),},
	{SEC_CMD("get_prox_cdc_array", get_prox_cdc_array),},
	{SEC_CMD("run_cs_raw_read_all", run_cs_raw_read_all), },
	{SEC_CMD("run_cs_delta_read_all", run_cs_delta_read_all), },
	{SEC_CMD("get_max_dcm", get_max_dcm), },
	{SEC_CMD("get_cm_all_data", get_cm_all_data),},
	{SEC_CMD("run_cm_read_all", run_cm_read_all),},
	{SEC_CMD("get_slope0_all_data", get_slope0_all_data),},
	{SEC_CMD("get_slope1_all_data", get_slope1_all_data),},
	{SEC_CMD("get_cs_all_data", get_cs_all_data),},
	{SEC_CMD("run_cm_test", run_cm_test),},
	{SEC_CMD("get_cm_value", get_cm_value),},
	{SEC_CMD("get_cm_maxgap_value", get_cm_maxgap_value),},
	{SEC_CMD("get_cm_maxgap_all", get_cm_maxgap_all),},
	{SEC_CMD("run_cm_maxgap_read_all", get_cm_maxgap_all),},
	{SEC_CMD("get_tx_cm_gap_value", get_tx_cm_gap_value),},
	{SEC_CMD("get_rx_cm_gap_value", get_rx_cm_gap_value),},
	{SEC_CMD("run_jitter_read", run_cmjit_test),},
	{SEC_CMD("get_jitter", get_cmjit_value),},
	{SEC_CMD("run_cmcs_test", run_cmcs_test),},
	{SEC_CMD("get_cm_array", get_cm_array),},
	{SEC_CMD("get_slope0_array", get_slope0_array),},
	{SEC_CMD("get_slope1_array", get_slope1_array),},
	{SEC_CMD("get_cs_array", get_cs_array),},
	{SEC_CMD("get_cs0_array", get_cs_array),},
	{SEC_CMD("get_cs1_array", get_cs_array),},
	{SEC_CMD("run_cmcs_full_test", run_cmcs_full_test),},
	{SEC_CMD("run_trx_short_test", run_cmcs_full_test),},
	{SEC_CMD("get_config_ver", get_config_ver),},
	{SEC_CMD("run_force_calibration", run_force_calibration),},
	{SEC_CMD("get_force_calibration", get_force_calibration),},
	{SEC_CMD("run_mis_cal_read", run_miscalibration),},
	{SEC_CMD("run_mis_cal_read_all", run_miscalibration_all),},
	{SEC_CMD("get_mis_cal", get_miscalibration_value),},
	{SEC_CMD("get_mis_cal_info", get_mis_cal_info), },
	{SEC_CMD("check_connection", check_connection),},
	{SEC_CMD("run_prox_intensity_read_all", run_prox_intensity_read_all),},
#ifdef TCLM_CONCEPT
	{SEC_CMD("set_external_factory", set_external_factory),},
	{SEC_CMD("get_pat_information", get_pat_information),},
	{SEC_CMD("get_tsp_test_result", get_tsp_test_result),},
	{SEC_CMD("set_tsp_test_result", set_tsp_test_result),},
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	{SEC_CMD("ium_read", ist40xx_read_sec_info_all),},
#endif
	{SEC_CMD("increase_disassemble_count", increase_disassemble_count),},
	{SEC_CMD("get_disassemble_count", get_disassemble_count),},
#endif
	{SEC_CMD("check_ic_mode", check_ic_mode),},
	{SEC_CMD("get_wet_mode", get_wet_mode),},
	{SEC_CMD("get_crc_check", run_fw_integrity),},
	{SEC_CMD("get_fw_integrity", get_checksum_data),},
	{SEC_CMD("factory_cmd_result_all", factory_cmd_result_all),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

static ssize_t get_lp_dump(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int i, ret;
	u8 string_data[12] = {0, };
	u16 current_index;
	u8 dump_format, dump_num;
	u16 dump_start, dump_end;
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	if (data->status.sys_mode == STATE_POWER_OFF) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		return snprintf(buf, PAGE_SIZE, "TSP turned off");
	}

	ist40xx_disable_irq(data);

	ist40xx_cmd_hold(data, IST40XX_ENABLE);

	ret = ist40xx_burst_read(data->client,
			IST40XX_SPONGE_REG_BASE + IST40XX_SPONGE_LP_DUMP, (u32*)string_data,
			1, true);
	if (ret < 0) {
		input_err(true, &data->client->dev, "%s: Failed to read rect\n",
			  __func__);
		snprintf(buf, PAGE_SIZE, "NG, Failed to read rect");

		goto out;
	}

	dump_format = string_data[0];
	dump_num = string_data[1];
	dump_start = IST40XX_SPONGE_LP_DUMP + 4;
	dump_end = dump_start + (dump_format * (dump_num - 1));

	current_index = (string_data[3] & 0xFF) << 8 | (string_data[2] & 0xFF);
	if (current_index > dump_end || current_index < dump_start) {
		input_err(true, &data->client->dev, "Failed to Sponge LP log %d\n",
			  current_index);
		snprintf(buf, PAGE_SIZE, "NG, Failed to Sponge LP log, current_index=%d",
			 current_index);

		goto out;
	}

	input_info(true, &data->client->dev,
		   "%s: DEBUG fmt=%d, num=%d, start=%d, end=%d, current_index=%d\n",
		   __func__, dump_format, dump_num, dump_start, dump_end,
		   current_index);

	for (i = (dump_num - 1); i >= 0; i--) {
		u16 data0, data1, data2, data3, data4;
		const int msg_len = 128;
		char buff[msg_len] = {0, };
		u16 string_addr;
		u16 offset = 0;

		string_addr = current_index - (dump_format * i);
		if (current_index < (dump_format * i))
			string_addr += (dump_format * dump_num);

		if (string_addr < dump_start)
			string_addr += (dump_format * dump_num);

		if (dump_format == 8) {
			ret = ist40xx_burst_read(data->client,
				IST40XX_SPONGE_REG_BASE + string_addr, (u32*)string_data,
				dump_format / IST40XX_DATA_LEN, true);
			if (ret < 0) {
				input_err(true, &data->client->dev,
					  "%s: Failed to read rect\n", __func__);
				snprintf(buf, PAGE_SIZE, "NG, Failed to read rect, addr=%d",
					 string_addr);

				goto out;
			}

			data0 = (string_data[1] & 0xFF) << 8 | (string_data[0] & 0xFF);
			data1 = (string_data[3] & 0xFF) << 8 | (string_data[2] & 0xFF);
			data2 = (string_data[5] & 0xFF) << 8 | (string_data[4] & 0xFF);
			data3 = (string_data[7] & 0xFF) << 8 | (string_data[6] & 0xFF);

			if (data0 || data1 || data2 || data3) {
				snprintf(buff, msg_len, "%d: %04x%04x%04x%04x\n",
					 string_addr, data0, data1, data2, data3);
				strncat(buf, buff, msg_len);
			}
		} else {
			if (string_addr % IST40XX_ADDR_LEN)
				offset = 2;

			ret = ist40xx_burst_read(data->client,
					IST40XX_SPONGE_REG_BASE + string_addr - offset,
					(u32*)string_data, (dump_format / IST40XX_DATA_LEN) + 1,
					true);
			if (ret < 0) {
				input_err(true, &data->client->dev,
					  "%s: Failed to read rect\n", __func__);
				snprintf(buf, PAGE_SIZE, "NG, Failed to read rect, addr=%d",
					 string_addr);

				goto out;
			}

			data0 = (string_data[1 + offset] & 0xFF) << 8 |
					(string_data[0 + offset] & 0xFF);
			data1 = (string_data[3 + offset] & 0xFF) << 8 |
					(string_data[2 + offset] & 0xFF);
			data2 = (string_data[5 + offset] & 0xFF) << 8 |
					(string_data[4 + offset] & 0xFF);
			data3 = (string_data[7 + offset] & 0xFF) << 8 |
					(string_data[6 + offset] & 0xFF);
			data4 = (string_data[9 + offset] & 0xFF) << 8 |
					(string_data[8 + offset] & 0xFF);

			if (data0 || data1 || data2 || data3 || data4) {
				snprintf(buff, msg_len, "%d: %04x%04x%04x%04x%04x\n",
						string_addr, data0, data1, data2, data3, data4);
				strncat(buf, buff, msg_len);
			}
		}
	}

out:
	ist40xx_cmd_hold(data, IST40XX_DISABLE);
	ist40xx_enable_irq(data);

	return strlen(buf);
}

static ssize_t scrub_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char buff[256] = { 0 };
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	input_info(true, &data->client->dev, "%s() scrub_id: %d, X:%d, Y:%d \n",
		   __func__, data->scrub_id, data->scrub_x, data->scrub_y);

	snprintf(buff, sizeof(buff), "%d %d %d", data->scrub_id, data->scrub_x,
		 data->scrub_y);

	data->scrub_id = 0;

	return snprintf(buf, PAGE_SIZE, "%s", buff);
}

static ssize_t read_ito_check_show(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	input_info(true, &data->client->dev, "%s: %02X%02X%02X%02X\n", __func__,
		   data->ito_test[0], data->ito_test[1], data->ito_test[2],
		   data->ito_test[3]);

	return snprintf(buf, PAGE_SIZE, "%02X%02X%02X%02X", data->ito_test[0],
			data->ito_test[1], data->ito_test[2], data->ito_test[3]);
}

static ssize_t read_raw_check_show(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	input_info(true, &data->client->dev, "%s\n", __func__);

	return snprintf(buf, PAGE_SIZE, "OK");
}

static ssize_t read_multi_count_show(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	char buffer[256] = { 0 };
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	input_err(true, &data->client->dev, "%s: %d\n", __func__, data->multi_count);
	snprintf(buffer, sizeof(buffer), "%d", data->multi_count);

	return snprintf(buf, PAGE_SIZE, "%s\n", buffer);
}

static ssize_t clear_multi_count_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	data->multi_count = 0;

	input_info(true, &data->client->dev, "%s: clear\n", __func__);

	return count;
}

static ssize_t read_wet_mode_show(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	char buffer[256] = { 0 };
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	input_info(true, &data->client->dev, "%s: %d\n", __func__,
		   data->wet_count);
	snprintf(buffer, sizeof(buffer), "%d", data->wet_count);

	return snprintf(buf, PAGE_SIZE, "%s\n", buffer);
}

static ssize_t clear_wet_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	data->wet_count = 0;

	input_info(true, &data->client->dev, "%s: clear\n", __func__);

	return count;
}

static ssize_t read_comm_err_count_show(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	char buffer[256] = { 0 };
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	input_info(true, &data->client->dev, "%s: %d\n", __func__,
		   data->comm_err_count);
	snprintf(buffer, sizeof(buffer), "%d", data->comm_err_count);

	return snprintf(buf, PAGE_SIZE, "%s\n", buffer);
}

static ssize_t clear_comm_err_count_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	data->comm_err_count = 0;

	input_info(true, &data->client->dev, "%s: clear\n", __func__);

	return count;
}

static ssize_t read_module_id_show(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

#ifdef TCLM_CONCEPT
	input_info(true, &data->client->dev, "%s: IM%04X%02X%02X%02X\n", __func__,
		   (data->fw.cur.fw_ver & 0xffff), data->test_result.data[0],
		   data->tdata->nvdata.cal_count,
		   (data->tdata->nvdata.tune_fix_ver & 0xff));

	return snprintf(buf, PAGE_SIZE, "IM%04X%02X%02X%02X",
			(data->fw.cur.fw_ver & 0xffff), data->test_result.data[0],
			data->tdata->nvdata.cal_count,
			(data->tdata->nvdata.tune_fix_ver & 0xff));
#else
	input_info(true, &data->client->dev, "%s: IM%04X\n", __func__,
		  (data->fw.cur.fw_ver & 0xffff));

	return snprintf(buf, PAGE_SIZE, "IM%04X", (data->fw.cur.fw_ver & 0xffff));
#endif
}

static ssize_t read_checksum_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	input_info(true, &data->client->dev, "%s: %d\n", __func__,
		   data->checksum_result);

	return snprintf(buf, PAGE_SIZE, "%d", data->checksum_result);
}

static ssize_t clear_checksum_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	data->checksum_result = 0;

	input_info(true, &data->client->dev, "%s: clear\n", __func__);

	return count;
}

static ssize_t read_all_touch_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	input_info(true, &data->client->dev, "%s: touch:%d, aod:%d, spay:%d\n",
		   __func__, data->all_finger_count, data->all_aod_tsp_count,
		   data->all_spay_count);

	return snprintf(buf, PAGE_SIZE,
			"\"TTCN\":\"%d\",\"TACN\":\"%d\",\"TSCN\":\"%d\"",
			data->all_finger_count, data->all_aod_tsp_count,
			data->all_spay_count);
}

static ssize_t clear_all_touch_count_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	data->all_aod_tsp_count = 0;
	data->all_spay_count = 0;
	data->all_singletap_count = 0;

	input_info(true, &data->client->dev, "%s: clear\n", __func__);

	return count;
}

static ssize_t read_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s", TSP_CHIP_VENDOR);
}

static ssize_t sensitivity_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	int mode;

	if (kstrtoint(buf, 10, &mode) < 0) {
		input_err(true, &data->client->dev, "%s: kstrtoint fail\n", __func__);
		return count;
	}

	if ((mode != 0) && (mode != 1))	// enable/disable
		return count;

	ist40xx_set_sensitivity_mode(mode);

	return count;
}

static ssize_t sensitivity_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u32 result[5] = { 0, };
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	if (data->status.sys_mode == STATE_POWER_OFF) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is not STATE_POWER_ON!\n",
			  __func__);
		goto end;
	}

	if (data->noise_mode & (1 << NOISE_MODE_SENSITIVITY))
		ist40xx_burst_read(data->client, IST40XX_HIB_INTR_MSG, result, 5, true);

end:
	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d", result[0], result[1],
					result[2], result[3], result[4]);
}

static ssize_t ear_detect_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	int mode;

	if (kstrtoint(buf, 10, &mode) < 0) {
		input_err(true, &data->client->dev, "%s: kstrtoint fail\n", __func__);
		return count;
	}

	if ((mode != 0) && (mode != 1))
		return count;

	ist40xx_set_call_mode(mode);

	return count;
}

static ssize_t ear_detect_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	input_info(true, &data->client->dev, "%s: %d\n",
		   __func__, data->noise_mode >> NOISE_MODE_CALL);

	return snprintf(buf, PAGE_SIZE, "%d", data->noise_mode >> NOISE_MODE_CALL);
}

static ssize_t prox_power_off_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	int mode;

	if (kstrtoint(buf, 10, &mode) < 0) {
		input_err(true, &data->client->dev, "%s: kstrtoint fail\n", __func__);
		return count;
	}

	if ((mode != 0) && (mode != 1))
		return count;

	input_info(true, &data->client->dev, "%s: enable:%d\n", __func__, mode);
	data->prox_power_off = mode;

	return count;
}

static ssize_t prox_power_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);

	input_info(true, &data->client->dev, "%s: %d\n", __func__, data->prox_power_off);

	return snprintf(buf, PAGE_SIZE, "%d", data->prox_power_off);
}

static ssize_t read_support_feature(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ist40xx_data *data = container_of(sec, struct ist40xx_data, sec);
	u32 feature = 0;

	if (data->dt_data->enable_settings_aot )
		feature |= INPUT_FEATURE_ENABLE_SETTINGS_AOT;

	input_info(true, &data->client->dev, "%s: %d%s\n", __func__, feature,
			feature & INPUT_FEATURE_ENABLE_SETTINGS_AOT ? " aot" : "");

	return snprintf(buf, PAGE_SIZE, "%d", feature);
}

/* sysfs - tsp */
static DEVICE_ATTR(close_tsp_test, S_IRUGO, show_close_tsp_test, NULL);
static DEVICE_ATTR(scrub_pos, S_IRUGO, scrub_position_show, NULL);
/* BiG data */
static DEVICE_ATTR(ito_check, S_IRUGO, read_ito_check_show, NULL);
static DEVICE_ATTR(raw_check, S_IRUGO, read_raw_check_show, NULL);
static DEVICE_ATTR(multi_count, S_IRUGO | S_IWUSR | S_IWGRP,
		read_multi_count_show, clear_multi_count_store);
static DEVICE_ATTR(wet_mode, S_IRUGO | S_IWUSR | S_IWGRP, read_wet_mode_show,
		clear_wet_mode_store);
static DEVICE_ATTR(comm_err_count, S_IRUGO | S_IWUSR | S_IWGRP,
		read_comm_err_count_show, clear_comm_err_count_store);
static DEVICE_ATTR(checksum, S_IRUGO | S_IWUSR | S_IWGRP, read_checksum_show,
		clear_checksum_store);
static DEVICE_ATTR(all_touch_count, S_IRUGO | S_IWUSR | S_IWGRP,
		read_all_touch_count_show, clear_all_touch_count_store);
static DEVICE_ATTR(module_id, S_IRUGO, read_module_id_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, read_vendor_show, NULL);
static DEVICE_ATTR(get_lp_dump, S_IRUGO, get_lp_dump, NULL);
static DEVICE_ATTR(sensitivity_mode, S_IRUGO | S_IWUSR | S_IWGRP,
		sensitivity_mode_show, sensitivity_mode_store);
static DEVICE_ATTR(ear_detect_enable, S_IRUGO | S_IWUSR | S_IWGRP,
		ear_detect_enable_show, ear_detect_enable_store);
static DEVICE_ATTR(prox_power_off, S_IRUGO | S_IWUSR | S_IWGRP,
		prox_power_off_show, prox_power_off_store);
static DEVICE_ATTR(support_feature, S_IRUGO, read_support_feature, NULL);

static struct attribute *sec_touch_factory_attributes[] = {
	&dev_attr_close_tsp_test.attr,
	&dev_attr_scrub_pos.attr,
	&dev_attr_ito_check.attr,
	&dev_attr_raw_check.attr,
	&dev_attr_multi_count.attr,
	&dev_attr_wet_mode.attr,
	&dev_attr_comm_err_count.attr,
	&dev_attr_checksum.attr,
	&dev_attr_all_touch_count.attr,
	&dev_attr_module_id.attr,
	&dev_attr_vendor.attr,
	&dev_attr_get_lp_dump.attr,
	&dev_attr_sensitivity_mode.attr,
	&dev_attr_ear_detect_enable.attr,
	&dev_attr_prox_power_off.attr,
	&dev_attr_support_feature.attr,
	NULL,
};

static struct attribute_group sec_touch_factory_attr_group = {
	.attrs = sec_touch_factory_attributes,
};

#if defined(CONFIG_INPUT_SEC_SECURE_TOUCH)
static ssize_t ist40xx_secure_touch_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ist40xx_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", atomic_read(&data->st_enabled));
}

/*
 * Accept only "0" and "1" valid values.
 * "0" will reset the st_enabled flag, then wake up the reading process.
 * The bus driver is notified via pm_runtime that it is not required to stay
 * awake anymore.
 * It will also make sure the queue of events is emptied in the controller,
 * in case a touch happened in between the secure touch being disabled and
 * the local ISR being ungated.
 * "1" will set the st_enabled flag and clear the st_pending_irqs flag.
 * The bus driver is requested via pm_runtime to stay awake.
 */
static ssize_t ist40xx_secure_touch_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ist40xx_data *data = dev_get_drvdata(dev);
	unsigned long value;
	int err = 0;

	if (count > 2) {
		input_err(true, &data->client->dev,
				"%s: cmd length is over (%s,%d)!!\n",
				__func__, buf, (int)strlen(buf));
		return -EINVAL;
	}

	err = kstrtoul(buf, 10, &value);
	if (err != 0) {
		input_err(true, &data->client->dev, "%s: failed to read:%d\n",
				__func__, err);
		return err;
	}

	err = count;

	switch (value) {
	case 0:
		if (atomic_read(&data->st_enabled) == 0) {
			input_err(true, &data->client->dev, "%s: secure_touch is not enabled, pending:%d\n",
					__func__, atomic_read(&data->st_pending_irqs));
			break;
		}

		pm_runtime_put_sync(data->client->adapter->dev.parent);

		atomic_set(&data->st_enabled, 0);

		sysfs_notify(&data->input_dev->dev.kobj, NULL, "secure_touch");

		ist40xx_delay(10);

		ist40xx_irq_thread(data->client->irq, data);

		complete(&data->st_powerdown);
		complete(&data->st_interrupt);

		input_info(true, &data->client->dev, "%s: secure_touch is disabled\n", __func__);

#ifdef IST40XX_NOISE_MODE
		schedule_delayed_work(&data->work_noise_protect, 0);
#endif
#if defined(CONFIG_TRUSTONIC_TRUSTED_UI_QC)
		complete(&data->st_irq_received);
#endif
		break;

	case 1:
//		if (data->reset_is_on_going) {
//			input_err(true, &info->client->dev, "%s: reset is on goning becuse i2c fail\n",
//					__func__);
//			return -EBUSY;
//		}

		if (atomic_read(&data->st_enabled)) {
			input_err(true, &data->client->dev, "%s: secure_touch is already enabled, pending:%d\n",
					__func__, atomic_read(&data->st_pending_irqs));
			err = -EBUSY;
			break;
		}

#ifdef IST40XX_NOISE_MODE
		cancel_delayed_work_sync(&data->work_noise_protect);
#endif

		/* synchronize_irq -> disable_irq + enable_irq
		 * concern about timing issue.
		 */
		ist40xx_disable_irq(data);
		/* Release All Finger */
		/*  TODO */
		if (pm_runtime_get_sync(data->client->adapter->dev.parent) < 0) {
			input_err(true, &data->client->dev, "%s: pm_runtime_get failed\n", __func__);
			err = -EIO;
			ist40xx_enable_irq(data);
			break;
		}

		reinit_completion(&data->st_powerdown);
		reinit_completion(&data->st_interrupt);
#if defined(CONFIG_TRUSTONIC_TRUSTED_UI_QC)
		reinit_completion(&data->st_irq_received);
#endif
		atomic_set(&data->st_enabled, 1);
		atomic_set(&data->st_pending_irqs, 0);

		ist40xx_enable_irq(data);

		input_info(true, &data->client->dev, "%s: secure_touch is enabled\n", __func__);

		break;

	default:
		input_err(true, &data->client->dev, "%s: unsupported value: %lu\n", __func__, value);
		err = -EINVAL;
		break;
	}
	return err;
}

#if defined(CONFIG_TRUSTONIC_TRUSTED_UI_QC)
static int secure_get_irq(struct device *dev)
{
	struct ist40xx_data *data = dev_get_drvdata(dev);
	int val = 0;

	input_err(true, &data->client->dev, "%s: enter\n", __func__);
	if (atomic_read(&data->st_enabled) == 0) {
		input_err(true, &data->client->dev, "%s: disabled\n", __func__);
		return -EBADF;
	}

	if (atomic_cmpxchg(&data->st_pending_irqs, -1, 0) == -1) {
		input_err(true, &data->client->dev, "%s: pending irq -1\n", __func__);
		return -EINVAL;
	}

	if (atomic_cmpxchg(&data->st_pending_irqs, 1, 0) == 1)
		val = 1;

	input_err(true, &data->client->dev, "%s: pending irq is %d\n",
			__func__, atomic_read(&data->st_pending_irqs));

	complete(&data->st_interrupt);

	return val;
}
#endif

static ssize_t ist40xx_secure_touch_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ist40xx_data *data = dev_get_drvdata(dev);
	int val = 0;

	if (atomic_read(&data->st_enabled) == 0) {
		input_err(true, &data->client->dev, "%s: secure_touch is not enabled, st_pending_irqs: %d\n",
				__func__, atomic_read(&data->st_pending_irqs));
		return -EBADF;
	}

	if (atomic_cmpxchg(&data->st_pending_irqs, -1, 0) == -1) {
		input_err(true, &data->client->dev, "%s: st_pending_irqs: %d\n",
				__func__, atomic_read(&data->st_pending_irqs));
		return -EINVAL;
	}

	if (atomic_cmpxchg(&data->st_pending_irqs, 1, 0) == 1) {
		val = 1;
		input_info(true, &data->client->dev, "%s: st_pending_irqs: %d, val: %d\n",
				__func__, atomic_read(&data->st_pending_irqs), val);
	}

	complete(&data->st_interrupt);

	return scnprintf(buf, PAGE_SIZE, "%u", val);
}

static void ist40xx_secure_touch_init(struct ist40xx_data *data)
{
	init_completion(&data->st_powerdown);
	init_completion(&data->st_interrupt);
#if defined(CONFIG_TRUSTONIC_TRUSTED_UI_QC)
	init_completion(&data->st_irq_received);
#endif
#if defined(CONFIG_TRUSTONIC_TRUSTED_UI_QC)
	register_tui_hal_ts(&data->input_dev->dev, &data->st_enabled,
			&data->st_irq_received, secure_get_irq,
			ist40xx_secure_touch_enable_store);
#endif
}

void ist40xx_secure_touch_stop(struct ist40xx_data *data, int blocking)
{
	if (atomic_read(&data->st_enabled)) {
		atomic_set(&data->st_pending_irqs, -1);
		sysfs_notify(&data->input_dev->dev.kobj, NULL, "secure_touch");
		if (blocking)
			wait_for_completion_interruptible(&data->st_powerdown);
	}
}

irqreturn_t ist40xx_filter_interrupt(struct ist40xx_data *data)
{
	if (atomic_read(&data->st_enabled)) {
		if (atomic_cmpxchg(&data->st_pending_irqs, 0, 1) == 0) {
			sysfs_notify(&data->input_dev->dev.kobj, NULL, "secure_touch");
		} else {
			input_info(true, &data->client->dev, "%s: st_pending_irqs: %d\n",
					__func__, atomic_read(&data->st_pending_irqs));
		}
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

static struct device_attribute secure_attrs[] = {
	__ATTR(secure_touch_enable, (0664),
			ist40xx_secure_touch_enable_show,
			ist40xx_secure_touch_enable_store),
	__ATTR(secure_touch, (0444),
			ist40xx_secure_touch_show,
			NULL),
};
#endif

int run_debug_for_dump(struct ist40xx_data *data)
{
	int ret;
	int i;
	u32 addr = IST40XX_DA_ADDR(data->algo_addr);
	u32 size = data->algo_size;
	u32 Scancount = 0;
	u32 *buf32;

	if (!addr) {
		addr = IST40XX_ALGORITHM_ADDR;
		size = 1024;
	}

	input_raw_info(true, &data->client->dev, "----- scancount value -----:\n");
	ist40xx_read_reg(data->client, IST40XX_HIB_TOUCH_STATUS, &Scancount);
	input_raw_info(true, &data->client->dev, "ScanCount(0) : %08X\n", Scancount);
	ist40xx_delay(100);
	ist40xx_read_reg(data->client, IST40XX_HIB_TOUCH_STATUS, &Scancount);
	input_raw_info(true, &data->client->dev, "ScanCount(1) : %08X\n", Scancount);
	ist40xx_delay(100);
	ist40xx_read_reg(data->client, IST40XX_HIB_TOUCH_STATUS, &Scancount);
	input_raw_info(true, &data->client->dev, "ScanCount(2) : %08X\n", Scancount);

	data->status.event_mode = false;
	ist40xx_cmd_hold(data, IST40XX_ENABLE);
	buf32 = kzalloc(size * sizeof(u32), GFP_KERNEL);
	ret = ist40xx_burst_read(data->client, addr, buf32, size, true);
	if (ret) {
		data->status.event_mode = true;
		ist40xx_reset(data, false);
		ist40xx_start(data);
		kfree(buf32);
		return ret;
	}
	ist40xx_cmd_hold(data, IST40XX_DISABLE);

	input_raw_info(true, &data->client->dev, "----- debug value -----:\n");
	for (i = 0; i < (size / IST40XX_DATA_LEN); i++)
		input_raw_info(true, &data->client->dev, "%08X %08X %08X %08X\n", buf32[i * 4], buf32[i * 4 + 1],
					buf32[i * 4 + 2], buf32[i * 4 + 3]);

	data->status.event_mode = true;
	kfree(buf32);

	return 0;
}

int run_cp_for_dump(struct ist40xx_data *data)
{
	int ret;
	TSP_INFO *tsp = &data->tsp_info;

	mutex_lock(&data->lock);
	ret = ist40xx_read_cp_node(data, &tsp->node, false);
	if (ret) {
		mutex_unlock(&data->lock);
		tsp_err("MemX cp read fail\n");
		return ret;
	}

	ist40xx_parse_cp_node(data, &tsp->node, false);

	ret = ist40xx_read_cp_node(data, &tsp->node, true);
	if (ret) {
		mutex_unlock(&data->lock);
		tsp_err("Info cp read fail\n");
		return ret;
	}
	mutex_unlock(&data->lock);

	ist40xx_parse_cp_node(data, &tsp->node, true);

	return 0;
}

void dump_self_prox_log(struct ist40xx_data *data, int type)
{
	int ii, jj;
	int val = 0;
	const int msg_len = 128;
	char msg[msg_len];
	char *buf = NULL;
	TSP_INFO *tsp = &data->tsp_info;
	struct TSP_NODE_BUF *node = &tsp->node;

	buf = kzalloc(2048, GFP_KERNEL);
	if (buf == NULL)
	return;
	buf[0] = '\0';

	if (tsp->dir.swap_xy) {
		for (ii = tsp->ch_num.rx - 1; ii >= 0; ii--) {
			for (jj = tsp->ch_num.tx - 1; jj >= 0; jj--) {
				if (type == LOG_MEMX_CP)
					val = node->memx_cp[jj * tsp->ch_num.rx + ii];
				else if (type == LOG_ROM_CP)
					val = node->rom_cp[jj * tsp->ch_num.rx + ii];
				else if (type == LOG_CDC)
					val = node->cdc[jj * tsp->ch_num.rx + ii];
				else if (type == LOG_BASE)
					val = node->base[jj * tsp->ch_num.rx + ii];
				snprintf(msg, msg_len, "%4d ", val);
				strncat(buf, msg, msg_len);
				if (jj == 0) {
					if (type == LOG_MEMX_CP)
						val = node->memx_self_cp[tsp->ch_num.tx + ii];
					else if (type == LOG_ROM_CP)
						val = node->rom_self_cp[tsp->ch_num.tx + ii];
					else if (type == LOG_CDC)
						val = node->self_cdc[tsp->ch_num.tx + ii];
					else if (type == LOG_BASE)
						val = node->self_base[tsp->ch_num.tx + ii];
					snprintf(msg, msg_len, "     %4d ", val);
					strncat(buf, msg, msg_len);
					if (type == LOG_MEMX_CP)
						val = node->memx_prox_cp[tsp->ch_num.tx + ii];
					else if (type == LOG_ROM_CP)
						val = node->rom_prox_cp[tsp->ch_num.tx + ii];
					else if (type == LOG_CDC)
						val = node->prox_cdc[tsp->ch_num.tx + ii];
					else if (type == LOG_BASE)
						val = node->prox_base[tsp->ch_num.tx + ii];
					snprintf(msg, msg_len, "%4d ", val);
					strncat(buf, msg, msg_len);
					snprintf(msg, msg_len, " | Rx%02d", ii);
					strncat(buf, msg, msg_len);
				}
			}
			input_raw_info(true, &data->client->dev, "%s\n", buf);

			if (ii == 0) {
				input_raw_info(true, &data->client->dev, "\n");
				buf[0] = '\0';
				for (jj = tsp->ch_num.tx - 1; jj >= 0; jj--) {
					if (type == LOG_MEMX_CP)
						val = node->memx_self_cp[jj];
					else if (type == LOG_ROM_CP)
						val = node->rom_self_cp[jj];
					else if (type == LOG_CDC)
						val = node->self_cdc[jj];
					else if (type == LOG_BASE)
						val = node->self_base[jj];
					snprintf(msg, msg_len, "%4d ", val);
					strncat(buf, msg, msg_len);
				}
				input_raw_info(true, &data->client->dev, "%s\n", buf);

				buf[0] = '\0';
				for (jj = tsp->ch_num.tx - 1; jj >= 0; jj--) {
					if (type == LOG_MEMX_CP)
						val = node->memx_prox_cp[jj];
					else if (type == LOG_ROM_CP)
						val = node->rom_prox_cp[jj];
					else if (type == LOG_CDC)
						val = node->prox_cdc[jj];
					else if (type == LOG_BASE)
						val = node->prox_base[jj];
					snprintf(msg, msg_len, "%4d ", val);
					strncat(buf, msg, msg_len);
				}
				input_raw_info(true, &data->client->dev, "%s\n", buf);

				buf[0] = '\0';
				for (jj = tsp->ch_num.tx + 2; jj >= 0; jj--) {
					snprintf(msg, msg_len, "-----");
					strncat(buf, msg, msg_len);
				}
				input_raw_info(true, &data->client->dev, "%s\n", buf);

				buf[0] = '\0';
				for (jj = tsp->ch_num.tx - 1; jj >= 0 ; jj--) {
					snprintf(msg, msg_len, "  %02d ", jj);
					strncat(buf, msg, msg_len);
				}
				snprintf(msg, msg_len, "                | Tx");
				strncat(buf, msg, msg_len);
				input_raw_info(true, &data->client->dev, "%s\n", buf);
			}
			buf[0] = '\0';
		}
	} else {
		for (ii = tsp->ch_num.tx - 1; ii >= 0; ii--) {
			for (jj = tsp->ch_num.rx - 1; jj >= 0; jj--) {
				if (type == LOG_MEMX_CP)
					val = node->memx_cp[ii * tsp->ch_num.rx + ii];
				else if (type == LOG_ROM_CP)
					val = node->rom_cp[ii * tsp->ch_num.rx + jj];
				else if (type == LOG_CDC)
					val = node->cdc[ii * tsp->ch_num.rx + jj];
				else if (type == LOG_BASE)
					val = node->base[ii * tsp->ch_num.rx + jj];
				snprintf(msg, msg_len, "%4d ", val);
				strncat(buf, msg, msg_len);
				if (jj == 0) {
					if (type == LOG_MEMX_CP)
						val = node->memx_self_cp[ii];
					else if (type == LOG_ROM_CP)
						val = node->rom_self_cp[ii];
					else if (type == LOG_CDC)
						val = node->self_cdc[ii];
					else if (type == LOG_BASE)
						val = node->self_base[ii];
					snprintf(msg, msg_len, "     %4d ", val);
					strncat(buf, msg, msg_len);
					if (type == LOG_MEMX_CP)
						val = node->memx_prox_cp[ii];
					else if (type == LOG_ROM_CP)
						val = node->rom_prox_cp[ii];
					else if (type == LOG_CDC)
						val = node->prox_cdc[ii];
					else if (type == LOG_BASE)
						val = node->prox_base[ii];
					snprintf(msg, msg_len, "%4d ", val);
					strncat(buf, msg, msg_len);

					snprintf(msg, msg_len, " | Tx%02d", ii);
					strncat(buf, msg, msg_len);
				}
			}
			input_raw_info(true, &data->client->dev, "%s\n", buf);

			if (ii == 0) {
			input_raw_info(true, &data->client->dev, "\n");
			buf[0] = '\0';
			for (jj = tsp->ch_num.rx - 1; jj >= 0; jj--) {
				if (type == LOG_MEMX_CP)
					val = node->memx_self_cp[tsp->ch_num.tx + jj];
				else if (type == LOG_ROM_CP)
					val = node->rom_self_cp[tsp->ch_num.tx + jj];
				else if (type == LOG_CDC)
					val = node->self_cdc[tsp->ch_num.tx + jj];
				else if (type == LOG_BASE)
					val = node->self_base[jj];
				snprintf(msg, msg_len, "%4d ", val);
				strncat(buf, msg, msg_len);
			}
			input_raw_info(true, &data->client->dev, "%s\n", buf);

			buf[0] = '\0';
			for (jj = tsp->ch_num.rx - 1; jj >= 0; jj--) {
				if (type == LOG_MEMX_CP)
					val = node->memx_prox_cp[tsp->ch_num.tx + jj];
				else if (type == LOG_ROM_CP)
					val = node->rom_prox_cp[tsp->ch_num.tx + jj];
				else if (type == LOG_CDC)
					val = node->prox_cdc[tsp->ch_num.tx + jj];
				else if (type == LOG_BASE)
					val = node->prox_base[jj];
				snprintf(msg, msg_len, "%4d ", val);
				strncat(buf, msg, msg_len);
			}
			input_raw_info(true, &data->client->dev, "%s\n", buf);

			buf[0] = '\0';
			for (jj = tsp->ch_num.rx + 2; jj >= 0; jj--) {
				snprintf(msg, msg_len, "-----");
				strncat(buf, msg, msg_len);
			}
			input_raw_info(true, &data->client->dev, "%s\n", buf);

			buf[0] = '\0';
			for (jj = tsp->ch_num.rx - 1; jj >= 0 ; jj--) {
				snprintf(msg, msg_len, "  %02d ", jj);
				strncat(buf, msg, msg_len);
			}
			snprintf(msg, msg_len, "                | Rx");
			strncat(buf, msg, msg_len);
			input_raw_info(true, &data->client->dev, "%s\n", buf);
			}
			buf[0] = '\0';
		}
	}
	kfree(buf);
}

void dump_log(struct ist40xx_data *data, int type)
{
	int ii, jj;
	int val = 0;
	const int msg_len = 128;
	char msg[msg_len];
	char *buf = NULL;
	TSP_INFO *tsp = &data->tsp_info;
	struct TSP_NODE_BUF *node = &tsp->node;

	buf = kzalloc(2048, GFP_KERNEL);
	if (buf == NULL)
	return;
	buf[0] = '\0';

	if (tsp->dir.swap_xy) {
		for (ii = tsp->ch_num.rx - 1; ii >= 0; ii--) {
			for (jj = tsp->ch_num.tx - 1; jj >= 0; jj--) {
				if (type == LOG_LOFS)
					val = node->lofs[jj * tsp->ch_num.rx + ii];
				else if (type == LOG_CM)
					val = cmcs_buf->cm[jj * tsp->ch_num.rx + ii];
				else if (type == LOG_GAP)
					val = cmcs_buf->slope[jj * tsp->ch_num.rx + ii];
				else if (type == LOG_CS)
					val = cmcs_buf->cs[jj * tsp->ch_num.rx + ii];
				else if (type == LOG_MISCAL)
					val = node->miscal[jj * tsp->ch_num.rx + ii];
				snprintf(msg, msg_len, "%4d ", val);
				strncat(buf, msg, msg_len);
				if (jj == 0) {
					snprintf(msg, msg_len, " | Rx%02d", ii);
					strncat(buf, msg, msg_len);
				}
			}
			input_raw_info(true, &data->client->dev, "%s\n", buf);
			if (ii == 0) {
				buf[0] = '\0';
				for (jj = tsp->ch_num.tx - 1; jj >= 0; jj--) {
					snprintf(msg, msg_len, "-----");
					strncat(buf, msg, msg_len);
				}
				input_raw_info(true, &data->client->dev, "%s\n", buf);

				buf[0] = '\0';
				for (jj = tsp->ch_num.tx - 1; jj >= 0 ; jj--) {
					snprintf(msg, msg_len, "  %02d ", jj);
					strncat(buf, msg, msg_len);
				}
				snprintf(msg, msg_len, " | Tx");
				strncat(buf, msg, msg_len);
				input_raw_info(true, &data->client->dev, "%s\n", buf);
			}
			buf[0] = '\0';
		}
	} else {
		for (ii = tsp->ch_num.tx - 1; ii >= 0; ii--) {
			for (jj = tsp->ch_num.rx - 1; jj >= 0; jj--) {
				if (type == LOG_LOFS)
					val = node->lofs[ii * tsp->ch_num.rx + jj];
				else if (type == LOG_CM)
					val = cmcs_buf->cm[ii * tsp->ch_num.rx + jj];
				else if (type == LOG_GAP)
					val = cmcs_buf->slope[ii * tsp->ch_num.rx + jj];
				else if (type == LOG_CS)
					val = cmcs_buf->cs[ii * tsp->ch_num.rx + jj];
				else if (type == LOG_MISCAL)
					val = node->miscal[ii * tsp->ch_num.rx + jj];
				snprintf(msg, msg_len, "%4d ", val);
				strncat(buf, msg, msg_len);
				if (jj == 0) {
					snprintf(msg, msg_len, " | Tx%02d", ii);
					strncat(buf, msg, msg_len);
				}
			}
			input_raw_info(true, &data->client->dev, "%s\n", buf);
			if (ii == 0) {
				buf[0] = '\0';
				for (jj = tsp->ch_num.rx - 1; jj >= 0; jj--) {
					snprintf(msg, msg_len, "-----");
					strncat(buf, msg, msg_len);
				}
				input_raw_info(true, &data->client->dev, "%s\n", buf);

				buf[0] = '\0';
				for (jj = tsp->ch_num.rx - 1; jj >= 0 ; jj--) {
					snprintf(msg, msg_len, "  %02d ", jj);
					strncat(buf, msg, msg_len);
				}
				snprintf(msg, msg_len, " | Rx");
				strncat(buf, msg, msg_len);
				input_raw_info(true, &data->client->dev, "%s\n", buf);
			}
			buf[0] = '\0';
		}
	}

	kfree(buf);
}

int run_node_for_dump(struct ist40xx_data *data)
{
	int ret;
	int pre_mode = 0;
	u8 flag = NODE_FLAG_CDC | NODE_FLAG_BASE | NODE_FLAG_LOFS;
	TSP_INFO *tsp = &data->tsp_info;

	pre_mode = (data->noise_mode >> NOISE_MODE_CALL) & 1;
	ist40xx_set_call_mode(1);
	ist40xx_delay(300);

	mutex_lock(&data->lock);
	ret = ist40xx_read_touch_node(data, flag, &tsp->node);
	if (ret) {
		mutex_unlock(&data->lock);
		ist40xx_set_call_mode(pre_mode);
		tsp_err("node frame read fail\n");
		return ret;
	}
	mutex_unlock(&data->lock);
	ist40xx_set_call_mode(pre_mode);

	ist40xx_parse_touch_node(data, &tsp->node);

	return 0;
}

void dump_node_log(struct ist40xx_data *data)
{
	input_raw_info(true, &data->client->dev, "----- cdc value -----:\n");
	dump_self_prox_log(data, LOG_CDC);

	input_raw_info(true, &data->client->dev, "----- base value -----:\n");
	dump_self_prox_log(data, LOG_BASE);

	input_raw_info(true, &data->client->dev, "----- lofs value -----:\n");
	dump_log(data, LOG_LOFS);
}

int run_cmcs_for_dump(struct ist40xx_data *data)
{
	int ret;

	ret = ist40xx_get_cmcs_info(ts_cmcs_bin, ts_cmcs_bin_size);
	if (ret) {
		tsp_err("%s() get cmcs info read fail!\n", __func__);
		return ret;
	}

	mutex_lock(&data->lock);
	ret = ist40xx_cmcs_test(data,
				CMCS_FLAG_CM | CMCS_FLAG_SLOPE | CMCS_FLAG_CS);
	if (ret) {
		mutex_unlock(&data->lock);
		tsp_err( "%s() cmcs test fail!\n", __func__);
		return ret;
	}
	mutex_unlock(&data->lock);

	return 0;
}

void dump_cmcs_log(struct ist40xx_data *data)
{
	bool cm_result, cs_result, gap_result;
	
	input_raw_info(true, &data->client->dev, "----- cm value -----:\n");
	dump_log(data, LOG_CM);

	if (cmcs_buf->cm_result == 0) {
		cm_result = 0;
		input_raw_info(true, &data->client->dev,  "### CM result: pass ###\n");
	} else {
		cm_result = 1;
		input_raw_info(true, &data->client->dev, "### CM result: fail ###\n");
	}

	input_raw_info(true, &data->client->dev, "----- gap value -----:\n");
	dump_log(data, LOG_GAP);
		
	if (cmcs_buf->slope_result == 0) {
		gap_result = 0;
		tsp_err( "### GAP result: pass ###\n");
	} else {
		gap_result = 1;
		tsp_err( "### GAP result: fail ###\n");
	}

	input_raw_info(true, &data->client->dev, "----- cs value -----:\n");
	dump_log(data, LOG_CS);

	/* CS result */
	if (cmcs_buf->cs_result == 0) {
		cs_result = 0;
		input_raw_info(true, &data->client->dev, "### CS result: pass ###\n");
	} else {
		cs_result = 1;
		input_raw_info(true, &data->client->dev, "### CS result: fail ###\n");
	}
}

void dump_miscal_log(struct ist40xx_data *data)
{
	dump_log(data, LOG_MISCAL);
	/*for max miscal_value */
	input_raw_info(true, &data->client->dev, "### miscal_value - Max: %d ###\n", data->status.miscalib_result);

}

void ist40xx_display_booting_dump_log(struct ist40xx_data *data)
{
	int ret;

	input_raw_info(true, &data->client->dev, "*** TSP Dump CMCS Value ***\n");
	ret = run_cmcs_for_dump(data);
	if (ret) {
		input_raw_info(true, &data->client->dev, "TSP Dump CMCS FAILED\n");
		return;
	}
	dump_cmcs_log(data);

	ret = ist40xx_miscalibrate(data);
	if (ret) {
		input_raw_info(true, &data->client->dev, "TSP Dump Miscal FAILED\n");
		return;
	}
	input_raw_info(true, &data->client->dev, "*** TSP Dump Miscal Value ***\n");
	dump_miscal_log(data);
}

void ist40xx_display_key_dump_log(struct ist40xx_data *data)
{
	int ret;

	ret = run_debug_for_dump(data);
	if (ret) {
		input_raw_info(true, &data->client->dev, "TSP Dump Debug FAILED\n");
		return;
	}

	ret = run_node_for_dump(data);
	if (ret) {
		input_raw_info(true, &data->client->dev, "TSP Dump Node FAILED\n");
		return;
	}

	input_raw_info(true, &data->client->dev, "----- cdc value -----:\n");
	dump_self_prox_log(data, LOG_CDC);

	input_raw_info(true, &data->client->dev, "----- base value -----:\n");
	dump_self_prox_log(data, LOG_BASE);
	
	input_raw_info(true, &data->client->dev, "----- lofs value -----:\n");
	dump_log(data, LOG_LOFS);

	ret = run_cp_for_dump(data);
	if (ret) {
		input_raw_info(true, &data->client->dev, "TSP Dump CP FAILED\n");
		return;
	}

	input_raw_info(true, &data->client->dev, "----- MemX cp value -----:\n");
	dump_self_prox_log(data, LOG_MEMX_CP);

	input_raw_info(true, &data->client->dev, "----- Info cp value -----:\n");
	dump_self_prox_log(data, LOG_ROM_CP);

	ret = run_cmcs_for_dump(data);
	if (ret) {
		input_raw_info(true, &data->client->dev, "TSP Dump CMCS FAILED\n");
		return;
	}
	dump_cmcs_log(data);

	ret = ist40xx_miscalibrate(data);
	if (ret) {
		input_raw_info(true, &data->client->dev, "TSP Dump Miscal FAILED\n");
		return;
	}
	input_raw_info(true, &data->client->dev, "*** TSP Dump Miscal Value ***\n");
	dump_miscal_log(data);

}

int sec_touch_sysfs(struct ist40xx_data *data)
{
	int ret;
#if defined(CONFIG_INPUT_SEC_SECURE_TOUCH)
	int i;
#endif

	/* /sys/class/sec/tsp */
	ret = sec_cmd_init(&data->sec, sec_cmds,
			   ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
	if (ret < 0) {
		input_err(true, &data->client->dev, "%s: Failure in sec_cmd_init\n",
			  __func__);
		return ret;
	}

	ret = sysfs_create_link(&data->sec.fac_dev->kobj,
			&data->input_dev->dev.kobj, "input");
	if (ret < 0) {
		input_err(true, &data->client->dev,
			  "%s: Failed to create input symbolic link\n", __func__);
		goto err_sec_fac_dev_link;
	}

	/* /sys/class/sec/tsp/... */
	if (sysfs_create_group(&data->sec.fac_dev->kobj,
				&sec_touch_factory_attr_group)) {
		input_err(true, &data->client->dev, "%s: Failed to create sysfs group(%s)!\n",
			  __func__, SEC_CLASS_DEV_NAME_TSP);
		goto err_sec_fac_dev_attr;
	}

#if defined(CONFIG_INPUT_SEC_SECURE_TOUCH)
	for (i = 0; i < (int)ARRAY_SIZE(secure_attrs); i++) {
		ret = sysfs_create_file(&data->input_dev->dev.kobj,
				&secure_attrs[i].attr);
		if (ret < 0) {
			input_err(true, &data->client->dev,
					"%s: Failed to create sysfs attributes\n",
					__func__);
		}
	}

	ist40xx_secure_touch_init(data);
#endif
	return 0;

err_sec_fac_dev_attr:
	sysfs_remove_link(&data->sec.fac_dev->kobj, "input");
err_sec_fac_dev_link:
	sec_cmd_exit(&data->sec, SEC_CLASS_DEVT_TSP);

	return -ENODEV;
}

EXPORT_SYMBOL(sec_touch_sysfs);

void sec_touch_sysfs_remove(struct ist40xx_data *data)
{
#if defined(CONFIG_INPUT_SEC_SECURE_TOUCH)
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(secure_attrs); i++)
		sysfs_remove_file(&data->input_dev->dev.kobj, &secure_attrs[i].attr);
#endif

	sysfs_remove_group(&data->sec.fac_dev->kobj, &sec_touch_factory_attr_group);
	sysfs_remove_link(&data->sec.fac_dev->kobj, "input");
	sec_cmd_exit(&data->sec, SEC_CLASS_DEVT_TSP);
}

EXPORT_SYMBOL(sec_touch_sysfs_remove);
#endif
