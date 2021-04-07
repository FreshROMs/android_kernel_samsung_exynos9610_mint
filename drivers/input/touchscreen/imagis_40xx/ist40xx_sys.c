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

#include <asm/io.h>
#include <asm/unaligned.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>

#include "ist40xx.h"
#include "ist40xx_update.h"

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
#include <linux/trustedui.h>
#endif

/******************************************************************************
 * Return value of Error
 * EPERM  : 1 (Operation not permitted)
 * ENOENT : 2 (No such file or directory)
 * EIO    : 5 (I/O error)
 * ENXIO  : 6 (No such device or address)
 * EINVAL : 22 (Invalid argument)
 *****************************************************************************/
int ist40xx_write_sponge_reg(struct ist40xx_data *data, u16 idx, u16 *buf16,
		int len, bool hold)
{
	int i;
	int ret = -EIO;

	if (hold)
		ist40xx_cmd_hold(data, IST40XX_ENABLE);

	data->ignore_delay = true;

	for (i = 0; i < len; i++) {
		ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
				(eHCOM_SET_SP_REG_IDX << 16) | (idx + (sizeof(u16) * i)));
		if (ret) {
			input_err(true, &data->client->dev, "%s: fail to write idx\n",
				  __func__);
			goto err_write_sponge_reg;
		}

		ist40xx_delay(1);

		ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
				(eHCOM_SET_SP_REG_W_DATA << 16) | *(buf16 + i));
		if (ret) {
			input_err(true, &data->client->dev, "%s: fail to write data\n",
				  __func__);
			goto err_write_sponge_reg;
		}
	}

err_write_sponge_reg:

	data->ignore_delay = false;

	if (hold)
		ist40xx_cmd_hold(data, IST40XX_DISABLE);

	ist40xx_delay(1);

	return ret;
}

int ist40xx_read_sponge_reg(struct ist40xx_data *data, u16 idx, u16 *buf16,
		int len, bool hold)
{
	int i;
	int ret = -EIO;
	u32 rdata = 0;

	if (hold)
		ist40xx_cmd_hold(data, IST40XX_ENABLE);

	data->ignore_delay = true;

	for (i = 0; i < len; i++) {
		ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
				(eHCOM_SET_SP_REG_IDX << 16) | (idx + (sizeof(u16) * i)));
		if (ret) {
			input_err(true, &data->client->dev, "%s: fail to write idx\n",
				  __func__);
			goto err_read_sponge_reg;
		}

		ist40xx_delay(1);

		ret = ist40xx_read_reg(data->client, IST40XX_SPONGE_REG_R_DATA,
				&rdata);
		if (ret) {
			input_err(true, &data->client->dev, "%s: fail to read data\n",
				  __func__);
			goto err_read_sponge_reg;
		}

		*(buf16 + i) = rdata & 0xFFFF;
	}

err_read_sponge_reg:

	data->ignore_delay = false;

	if (hold)
		ist40xx_cmd_hold(data, IST40XX_DISABLE);

	ist40xx_delay(1);

	return ret;
}

int ist40xx_cmd_gesture(struct ist40xx_data *data, u16 value)
{
	int ret = -EIO;
	struct timeval utc_time;

	ret = ist40xx_write_cmd(data, IST40XX_HIB_INTR_MSG, IST40XX_LPM_VALUE);
	if (ret)
		input_err(true, &data->client->dev, "fail to write LPM TAG value.\n");

	if (value == IST40XX_ENABLE) {
		input_info(true, &data->client->dev, "%s\n", __func__);

		ist40xx_set_halfaod_mode(data->prox_power_off);

		do_gettimeofday(&utc_time);

		input_info(true, &data->client->dev, "Write UTC to Sponge = %X\n",
			   (int)utc_time.tv_sec);

		ist40xx_cmd_hold(data, IST40XX_ENABLE);
		ist40xx_write_sponge_reg(data, IST40XX_SPONGE_UTC,
				(u16 *)&utc_time.tv_sec, 2, false);
		ist40xx_write_sponge_reg(data, IST40XX_SPONGE_RECT, data->rect_data,
				4, false);

		data->ignore_delay = true;
 		ist40xx_write_cmd(data, IST40XX_HIB_CMD,
 				(eHCOM_NOTIRY_G_REGMAP << 16) | IST40XX_ENABLE);
		data->ignore_delay = false;

		ist40xx_cmd_hold(data, IST40XX_DISABLE);
	}

	ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			(eHCOM_GESTURE_EN << 16) | (value & 0xFFFF));
	if (ret) {
		input_err(true, &data->client->dev, "fail to write gesture command.\n");
	} else {
		if (value == IST40XX_ENABLE) {
			input_info(true, &data->client->dev,
				"%s: spay : %d, aod : %d, singletap : %d, aot : %d\n", __func__,
				   (data->lpm_mode & IST40XX_SPAY) ? 1 : 0,
				   (data->lpm_mode & IST40XX_AOD) ? 1 : 0,
				(data->lpm_mode & IST40XX_SINGLETAP) ? 1 : 0,
				(data->lpm_mode & IST40XX_DOUBLETAP_WAKEUP) ? 1 : 0);
		} else {
			input_err(true, &data->client->dev, "%s: normal mode\n",
				  __func__);
		}
	}

	return ret;
}

int ist40xx_cmd_start_scan(struct ist40xx_data *data)
{
	int ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			(eHCOM_FW_START << 16) | (IST40XX_ENABLE & 0xFFFF));

	data->status.noise_mode = true;

	return ret;
}

int ist40xx_cmd_calibrate(struct ist40xx_data *data)
{
	int ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			(eHCOM_RUN_CAL_AUTO << 16));

	input_info(true, &data->client->dev, "%s\n", __func__);

	return ret;
}

int ist40xx_cmd_miscalibrate(struct ist40xx_data *data)
{
	int ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			(eHCOM_RUN_CAL_MIS << 16) | (IST40XX_ENABLE & 0xFFFF));

	input_info(true, &data->client->dev, "%s\n", __func__);

	return ret;
}

int ist40xx_cmd_hold(struct ist40xx_data *data, int enable)
{
	int ret;

	if (!data->initialized || (data->status.update == 1))
		return 0;

	ret = ist40xx_write_cmd(data, IST40XX_HIB_CMD,
			(eHCOM_FW_HOLD << 16) | (enable & 0xFFFF));

	input_info(true, &data->client->dev, "%s: FW HOLDE %s\n", __func__,
		   enable ? "Enable" : "Disable");

	return ret;
}

int ist40xx_i2c_transfer(struct i2c_client *client, struct i2c_msg *msgs,
		int msg_num, u8 *cmd_buf)
{
	int ret = 0;
	int idx = msg_num - 1;
	int size = msgs[idx].len;
	u8 *msg_buf = NULL;
	u8 *pbuf = NULL;
	int trans_size, max_size = 0;

#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	struct ist40xx_data *data = i2c_get_clientdata(client);

	if (atomic_read(&data->st_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &client->dev,
			  "%s: TSP no accessible from Linux, TUI is enabled!\n",
			  __func__);
		return -EBUSY;
	}
#endif
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &client->dev, "%s: TSP no accessible from Linux, TUI is enabled!\n",
			  __func__);
		return -EIO;
	}
#endif

	if (msg_num == WRITE_CMD_MSG_LEN)
		max_size = I2C_MAX_WRITE_SIZE;
	else if (msg_num == READ_CMD_MSG_LEN)
		max_size = I2C_MAX_READ_SIZE;

	if (max_size == 0) {
		input_err(true, &client->dev, "%s: transaction size(%d)\n",
			  __func__, max_size);
		return -EINVAL;
	}

	if (msg_num == WRITE_CMD_MSG_LEN) {
		msg_buf = kmalloc(max_size + IST40XX_ADDR_LEN, GFP_KERNEL);
		if (!msg_buf)
			return -ENOMEM;
		memcpy(msg_buf, cmd_buf, IST40XX_ADDR_LEN);
		pbuf = msgs[idx].buf;
	}

	while (size > 0) {
		trans_size = (size >= max_size ? max_size : size);

		msgs[idx].len = trans_size;
		if (msg_num == WRITE_CMD_MSG_LEN) {
			memcpy(&msg_buf[IST40XX_ADDR_LEN], pbuf, trans_size);
			msgs[idx].buf = msg_buf;
			msgs[idx].len += IST40XX_ADDR_LEN;
		}
		ret = i2c_transfer(client->adapter, msgs, msg_num);
		if (ret != msg_num) {
			input_err(true, &client->dev,
				  "%s: i2c_transfer failed(%d), num=%d\n",
				  __func__, ret, msg_num);
			break;
		}

/*
		{
			int i;

			pr_info("sec_input: I2C: %s\n", __func__);
			pr_info("sec_input: I2C: W: ");
			for (i = 0; i < msgs[0].len; i++) {
				pr_cont("%02X ", msgs[0].buf[i]);
			}
			pr_cont("\n");

			if (msg_num > 1) {
				pr_info("sec_input: I2C: R: ");
				for (i = 0; i < msgs[1].len; i++) {
					pr_cont("%02X ", msgs[1].buf[i]);

				}
				pr_cont("\n");
			}
		}
*/
		if (msg_num == WRITE_CMD_MSG_LEN)
			pbuf += trans_size;
		else
			msgs[idx].buf += trans_size;

		size -= trans_size;
	}

	if (msg_num == WRITE_CMD_MSG_LEN)
		kfree(msg_buf);

	return ret;
}

int ist40xx_read_buf(struct i2c_client *client, u32 cmd, u32 *buf, u16 len)
{
	struct ist40xx_data *data = i2c_get_clientdata(client);
	int ret, i;
	u32 le_reg = cpu_to_be32(cmd);
	u32 *msg_buf = NULL;
	u8 *t_u8_reg;

	struct i2c_msg msg[READ_CMD_MSG_LEN] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = IST40XX_ADDR_LEN,
//			.buf = (u8 *)&le_reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len * IST40XX_DATA_LEN,
		},
	};

	t_u8_reg = kzalloc(4, GFP_KERNEL);
	if (!t_u8_reg)
		return -ENOMEM;

	t_u8_reg[3] = (u8)((le_reg >> 24) & 0xFF);
	t_u8_reg[2] = (u8)((le_reg >> 16) & 0xFF);
	t_u8_reg[1] = (u8)((le_reg >> 8) & 0xFF);
	t_u8_reg[0] = (u8)(le_reg & 0xFF);

	msg[0].buf = t_u8_reg;

	if (data->status.sys_mode == STATE_POWER_OFF) {
		input_err(true, &client->dev,
			  "%s: now sys_mode status is STATE_POWER_OFF!\n", __func__);
		kfree(t_u8_reg);
		return -ENODEV;
	}

	mutex_lock(&data->i2c_lock);

	msg_buf = kmalloc(len * IST40XX_DATA_LEN, GFP_KERNEL);
	memcpy(msg_buf, buf, len * IST40XX_DATA_LEN);
	msg[1].buf = (u8 *)msg_buf;

	ret = ist40xx_i2c_transfer(client, msg, READ_CMD_MSG_LEN, NULL);
	if (ret != READ_CMD_MSG_LEN) {
		data->comm_err_count++;
		kfree(msg_buf);
		kfree(t_u8_reg);
		mutex_unlock(&data->i2c_lock);
		return -EIO;
	}

	for (i = 0; i < len; i++)
		msg_buf[i] = cpu_to_be32(msg_buf[i]);

	memcpy(buf, msg_buf, len * IST40XX_DATA_LEN);
	kfree(msg_buf);
	kfree(t_u8_reg);
	mutex_unlock(&data->i2c_lock);

	return 0;
}

int ist40xx_write_buf(struct i2c_client *client, u32 cmd, u32 *buf, u16 len)
{
	struct ist40xx_data *data = i2c_get_clientdata(client);
	int i;
	int ret;
	struct i2c_msg msg;
	u8 *cmd_buf = NULL;
	u8 *msg_buf = NULL;

	if (data->status.sys_mode == STATE_POWER_OFF) {
		input_err(true, &client->dev,
			  "%s: now sys_mode status is STATE_POWER_OFF!\n", __func__);
		return -ENODEV;
	}

	cmd_buf = kmalloc(IST40XX_DATA_LEN, GFP_KERNEL);
	msg_buf = kmalloc((len + 1) * IST40XX_DATA_LEN, GFP_KERNEL);

	put_unaligned_be32(cmd, cmd_buf);

	if (len > 0) {
		for (i = 0; i < len; i++)
			put_unaligned_be32(buf[i], msg_buf + (i * IST40XX_DATA_LEN));
	} else {
		/* then add dummy data(4byte) */
		put_unaligned_be32(0, msg_buf);
		len = 1;
	}

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len * IST40XX_DATA_LEN;
	msg.buf = msg_buf;

	mutex_lock(&data->i2c_lock);

	ret = ist40xx_i2c_transfer(client, &msg, WRITE_CMD_MSG_LEN,
			cmd_buf);
	if (ret != WRITE_CMD_MSG_LEN) {
		data->comm_err_count++;
		mutex_unlock(&data->i2c_lock);
		kfree(cmd_buf);
		kfree(msg_buf);
		return -EIO;
	}

	mutex_unlock(&data->i2c_lock);

   	kfree(cmd_buf);
	kfree(msg_buf);

	return 0;
}

int ist40xx_read_reg(struct i2c_client *client, u32 reg, u32 *buf)
{
	struct ist40xx_data *data = i2c_get_clientdata(client);
	int ret;
	u32 le_reg = cpu_to_be32(reg);
	u32 *msg_buf = NULL;
	u8 *t_u8_reg;

	struct i2c_msg msg[READ_CMD_MSG_LEN] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = IST40XX_ADDR_LEN,
//			.buf = (u8 *)&le_reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = IST40XX_DATA_LEN,
		},
	};

	t_u8_reg = kzalloc(4, GFP_KERNEL);
	if (!t_u8_reg)
		return -ENOMEM;

	t_u8_reg[3] = (u8)((le_reg >> 24) & 0xFF);
	t_u8_reg[2] = (u8)((le_reg >> 16) & 0xFF);
	t_u8_reg[1] = (u8)((le_reg >> 8) & 0xFF);
	t_u8_reg[0] = (u8)(le_reg & 0xFF);

	msg[0].buf = t_u8_reg;
	
	if (data->status.sys_mode == STATE_POWER_OFF) {
		input_err(true, &client->dev,
			  "%s: now sys_mode status is STATE_POWER_OFF!\n", __func__);
		kfree(t_u8_reg);
		return -ENODEV;
	}

#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	if (atomic_read(&data->st_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &client->dev,
			  "%s: TSP no accessible from Linux, TUI is enabled!\n",
			  __func__);
		kfree(t_u8_reg);
		return -EBUSY;
	}
#endif
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &client->dev,
			  "%s: TSP no accessible from Linux, TUI is enabled!\n",
			  __func__);
		kfree(t_u8_reg);
		return -EIO;
	}
#endif

	mutex_lock(&data->i2c_lock);

	msg_buf = kmalloc(IST40XX_DATA_LEN, GFP_KERNEL);
	memcpy(msg_buf, buf, IST40XX_DATA_LEN);
	msg[1].buf = (u8 *)msg_buf;

	ret = i2c_transfer(client->adapter, msg, READ_CMD_MSG_LEN);
	if (ret != READ_CMD_MSG_LEN) {
		data->comm_err_count++;
		kfree(msg_buf);
		mutex_unlock(&data->i2c_lock);

		input_err(true, &client->dev, "%s: i2c failed (%d), cmd: %x\n",
			  __func__, ret, reg);
		kfree(t_u8_reg);
		return -EIO;
	}

/*
	{
		int i;

		pr_info("sec_input: I2C: %s\n", __func__);
		pr_info("sec_input: I2C: W: ");
		for (i = 0; i < msg[0].len; i++) {
			pr_cont("%02X ", msg[0].buf[i]);
		}
		pr_cont("\n");

		pr_info("sec_input: I2C: R: ");
		for (i = 0; i < msg[1].len; i++) {
			pr_cont("%02X ", msg[1].buf[i]);

		}
		pr_cont("\n");
	}
*/
	*buf = cpu_to_be32(*msg_buf);

	kfree(msg_buf);
	kfree(t_u8_reg);
	mutex_unlock(&data->i2c_lock);

	return 0;
}

int ist40xx_read_cmd(struct ist40xx_data *data, u32 cmd, u32 *buf)
{
	int ret;

	ret = ist40xx_cmd_hold(data, IST40XX_ENABLE);
	if (ret)
		return ret;

	ist40xx_read_reg(data->client, IST40XX_DA_ADDR(cmd), buf);

	ret = ist40xx_cmd_hold(data, IST40XX_DISABLE);
	if (ret) {
		ist40xx_reset(data, false);
		return ret;
	}

	return ret;
}

int ist40xx_write_cmd(struct ist40xx_data *data, u32 cmd, u32 val)
{
	int ret;
	struct i2c_msg msg;
	u8 *msg_buf = NULL;

	if (data->status.sys_mode == STATE_POWER_OFF) {
		input_err(true, &data->client->dev,
			  "%s: now sys_mode status is STATE_POWER_OFF!\n", __func__);
		return -ENODEV;
	}

#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	if (atomic_read(&data->st_enabled) == SECURE_TOUCH_ENABLED) {
		input_err(true, &data->client->dev,
			  "%s: TSP no accessible from Linux, TUI is enabled!\n",
			  __func__);
		return -EBUSY;
	}
#endif
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &data->client->dev,
			  "%s: TSP no accessible from Linux, TUI is enabled!\n",
			  __func__);
		return -EIO;
	}
#endif

	msg_buf = kmalloc(IST40XX_ADDR_LEN + IST40XX_DATA_LEN, GFP_KERNEL);

	put_unaligned_be32(cmd, msg_buf);
	put_unaligned_be32(val, msg_buf + IST40XX_ADDR_LEN);

	msg.addr = data->client->addr;
	msg.flags = 0;
	msg.len = IST40XX_ADDR_LEN + IST40XX_DATA_LEN;
	msg.buf = msg_buf;

	mutex_lock(&data->i2c_lock);

	ret = i2c_transfer(data->client->adapter, &msg, WRITE_CMD_MSG_LEN);
	if (ret != WRITE_CMD_MSG_LEN) {
		data->comm_err_count++;
		mutex_unlock(&data->i2c_lock);
		kfree(msg_buf);

		input_err(true, &data->client->dev, "%s: i2c failed (%d), cmd: %x(%x)\n",
			   __func__, ret, cmd, val);
		return -EIO;
	}
/*
	{
		int i;

		pr_info("sec_input: I2C: %s\n", __func__);
		pr_info("sec_input: I2C: W: ");
		for (i = 0; i < msg.len; i++) {
			pr_cont("%02X ", msg.buf[i]);
		}
		pr_cont("\n");
	}
*/
	if (data->initialized && (data->status.update != 1) && !data->ignore_delay)
		ist40xx_delay(50);
	else
		ist40xx_delay(1);

	mutex_unlock(&data->i2c_lock);
	kfree(msg_buf);

	return 0;
}

int ist40xx_burst_read(struct i2c_client *client, u32 addr, u32 *buf32, u16 len,
		bool bit_en)
{
	int ret = 0;
	int i;
	u16 max_len = I2C_MAX_READ_SIZE / IST40XX_DATA_LEN;
	u16 remain_len = len;

	if (bit_en)
		addr = IST40XX_BA_ADDR(addr);

	for (i = 0; i < len; i += max_len) {
		if (remain_len < max_len)
			max_len = remain_len;

		ret = ist40xx_read_buf(client, addr, buf32, max_len);
		if (ret) {
			input_err(true, &client->dev, "%s: Burst fail, addr: %x\n",
				  __func__, addr);
			return ret;
		}

		addr += max_len * IST40XX_DATA_LEN;
		buf32 += max_len;
		remain_len -= max_len;
	}

	return 0;
}

int ist40xx_burst_write(struct i2c_client *client, u32 addr,
		u32 *buf32, u16 len)
{
	int ret = 0;
	int i;
	u16 max_len = I2C_MAX_WRITE_SIZE / IST40XX_DATA_LEN;
	u16 remain_len = len;

	addr = IST40XX_BA_ADDR(addr);

	for (i = 0; i < len; i += max_len) {
		if (remain_len < max_len)
			max_len = remain_len;

		ret = ist40xx_write_buf(client, addr, buf32, max_len);
		if (ret) {
			input_err(true, &client->dev, "%s: Burst fail, addr: %x\n",
				  __func__, addr);
			return ret;
		}

		addr += max_len * IST40XX_DATA_LEN;
		buf32 += max_len;
		remain_len -= max_len;
	}

	return 0;
}

int ts_power_enable(struct ist40xx_data *data, int en)
{
	int ret = 0;

	input_info(true, &data->client->dev, "%s %s\n", __func__,
		   (en) ? "on" : "off");

	if(data->dt_data->is_power_by_gpio) {
		if (gpio_is_valid(data->dt_data->power_gpio)) {
			gpio_set_value(data->dt_data->power_gpio, en);
		}
	} else {
		struct regulator *regulator_avdd;

		regulator_avdd = regulator_get(NULL, data->dt_data->regulator_avdd);
		if (IS_ERR(regulator_avdd)) {
			input_err(true, &data->client->dev,
				  "%s: Failed to get %s regulator.\n", __func__,
				  data->dt_data->regulator_avdd);
			return PTR_ERR(regulator_avdd);
		}

		if (en) {
			ret = regulator_enable(regulator_avdd);
			if (ret) {
				input_err(true, &data->client->dev,
					  "%s: Failed to enable avdd: %d\n",
					  __func__, ret);
				return ret;
			}
		} else {
			if (regulator_is_enabled(regulator_avdd))
				regulator_disable(regulator_avdd);
		}

		regulator_put(regulator_avdd);
	}

	return ret;
}

int ist40xx_power_on(struct ist40xx_data *data, bool download)
{
	int ret = 0;

	if (data->status.sys_mode != STATE_POWER_OFF) {
		input_err(true, &data->client->dev,
			  "%s: now state is already power_on\n", __func__);
		return 0;
	}

	/* VDD enable */
	/* VDDIO enable */
	ret = ts_power_enable(data, 1);
	if (ret) {
		input_err(true, &data->client->dev,
			  "%s: ts_power_enable fail.\n", __func__);
		return ret;
	}

	if (download)
		ist40xx_delay(10);
	else
		ist40xx_delay(60);

	data->status.sys_mode = STATE_POWER_ON;
	data->ignore_delay = true;

	ret = ist40xx_set_i2c_32bit(data);
	if (ret) {
		input_err(true, &data->client->dev,
			  "%s: ist40xx_set_i2c_32bit fail.\n", __func__);
	}

	data->ignore_delay = false;

	return ret;
}

int ist40xx_power_off(struct ist40xx_data *data)
{
	int ret = 0;

	if (data->status.sys_mode == STATE_POWER_OFF) {
		input_err(true, &data->client->dev,
			  "%s: now state is already power_off \n", __func__);
		return 0;
	}

	/* VDDIO disable */
	/* VDD disable */
	data->status.sys_mode = STATE_POWER_OFF;

	ret = ts_power_enable(data, 0);
	if (ret) {
		input_err(true, &data->client->dev,
			  "%s: ts_power_enable fail.\n", __func__);
	}
	data->status.noise_mode = false;

	ist40xx_delay(30);

	return ret;
}

int ist40xx_reset(struct ist40xx_data *data, bool download)
{
	int ret = 0;
	int temp_sys_mode = data->status.sys_mode;

	input_info(true, &data->client->dev, "%s: temp_sys_mode:%d\n",
		   __func__, temp_sys_mode);

	ret = ist40xx_power_off(data);
	if (ret) {
		input_err(true, &data->client->dev,
			  "%s: ist40xx_power_off fail.\n", __func__);
		return ret;
	}

	ret = ist40xx_power_on(data, download);
	if (ret) {
		input_err(true, &data->client->dev,
			  "%s: ist40xx_power_on fail.\n", __func__);
		return ret;
	}

	ist40xx_write_sponge_reg(data, IST40XX_SPONGE_CTRL, (u16*)&data->lpm_mode,
			1, false);

	data->ignore_delay = true;
	ist40xx_write_cmd(data, IST40XX_HIB_CMD,
		(eHCOM_NOTIRY_G_REGMAP << 16) | IST40XX_ENABLE);
	data->ignore_delay = false;

	if (temp_sys_mode == STATE_LPM) {
		ist40xx_cmd_gesture(data, IST40XX_ENABLE);
		data->status.noise_mode = false;
		data->status.sys_mode = STATE_LPM;
	}

	return 0;
}

int ist40xx_init_system(struct ist40xx_data *data)
{
	int ret;

	// TODO : place additional code here.
	ret = ist40xx_reset(data, false);
	if (ret) {
		input_err(true, &data->client->dev,
			  "%s: ist40xx_reset failed (%d)\n", __func__, ret);
		return ret;
	}

	return 0;
}
