/*
 * Copyright 2015 NXP Semiconductors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 or later
 * as published by the Free Software Foundation.
 */

#include <stdlib.h>
#include <string.h>
#include "tfa_dsp_fw.h"
#include "dbgprint.h"
#include "NXP_I2C.h"
#include "tfa_internal.h"
#include "lxScribo.h"

/* translate a I2C driver error into an error */
static enum tfa98xx_error
tfa98xx_classify_i2c_error(enum NXP_I2C_Error i2c_error)
{
	switch (i2c_error) {
	case NXP_I2C_Ok:
		return TFA98XX_ERROR_OK;
	case NXP_I2C_NoAck:
	case NXP_I2C_ArbLost:
	case NXP_I2C_TimeOut:
		return TFA98XX_ERROR_I2C_NON_FATAL;
	default:
		return TFA98XX_ERROR_I2C_FATAL;
	}
}
/*
 * write a 16 bit subaddress
 */
enum tfa98xx_error
tfa98xx_write_register16(tfa98xx_handle_t handle,
			unsigned char subaddress, unsigned short value)
{
	enum NXP_I2C_Error i2c_error;
	unsigned char write_data[3]; /* subaddress and 2 bytes of the value */

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;

	write_data[0] = subaddress;
	write_data[1] = (value >> 8) & 0xFF;
	write_data[2] = value & 0xFF;

	i2c_error = NXP_I2C_WriteRead
		(handles_local[handle].slave_address, sizeof(write_data),
		 write_data, 0, NULL);

	return tfa98xx_classify_i2c_error(i2c_error);
}

enum tfa98xx_error
tfa98xx_read_register16(tfa98xx_handle_t handle,
		       unsigned char subaddress, unsigned short *p_value)
{
	enum NXP_I2C_Error i2c_error;
	unsigned char write_data[1]; /* subaddress */
	unsigned char read_buffer[2]; /* 2 data bytes */

	_ASSERT(p_value != NULL);
	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;
	write_data[0] = subaddress;
	read_buffer[0] = read_buffer[1] = 0;

	i2c_error = NXP_I2C_WriteRead
		(handles_local[handle].slave_address, sizeof(write_data),
		 write_data, sizeof(read_buffer), read_buffer);

	if (tfa98xx_classify_i2c_error(i2c_error) != TFA98XX_ERROR_OK)
		return tfa98xx_classify_i2c_error(i2c_error);

	*p_value = (read_buffer[0] << 8) + read_buffer[1];
	return TFA98XX_ERROR_OK;
}

enum tfa98xx_error
tfa98xx_read_data(tfa98xx_handle_t handle,
		 unsigned char subaddress, int num_bytes, unsigned char data[])
{
	enum NXP_I2C_Error i2c_error;
	unsigned char write_data[1]; /* subaddress */

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;
	if (num_bytes > handles_local[handle].buffer_size)
		return TFA98XX_ERROR_BAD_PARAMETER;

	write_data[0] = subaddress;
	i2c_error = NXP_I2C_WriteRead
		(handles_local[handle].slave_address, sizeof(write_data),
		write_data, num_bytes, data);
	return tfa98xx_classify_i2c_error(i2c_error);
}

/*
 * Write raw I2C data with no sub address
 */
enum tfa98xx_error
tfa98xx_write_raw(tfa98xx_handle_t handle,
		  int num_bytes,
		  const unsigned char data[])
{
	enum NXP_I2C_Error i2c_error;

	if (!tfa98xx_handle_is_open(handle))
		return TFA98XX_ERROR_NOT_OPEN;
	if (num_bytes > handles_local[handle].buffer_size)
		return TFA98XX_ERROR_BAD_PARAMETER;
	i2c_error = NXP_I2C_WriteRead
		(handles_local[handle].slave_address, num_bytes,
		data, 0, NULL);
	return tfa98xx_classify_i2c_error(i2c_error);
}
