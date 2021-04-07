/*
 *  Copyright (C) 2010, Imagis Technology Co. Ltd. All Rights Reserved.
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

#ifndef __IST40XX_UPDATE_H__
#define __IST40XX_UPDATE_H__

// Flash size
#define IST40XX_ROM_BASE_ADDR		(0)
#define IST40XX_ROM_TOTAL_SIZE		(0x14000)
#define IST40XX_ROM_PAGE_SIZE		(0x400)
#define IST40XX_IUM_BASE_ADDR		(0x400)
#define IST40XX_IUM_SIZE		(0x1400)

// EEPROM register
#define rISP_BASE			(0x40006000)
#define rISP_ACCESS_MODE		IST40XX_DA_ADDR(rISP_BASE | 0x00)
#define rISP_ADDRESS			IST40XX_DA_ADDR(rISP_BASE | 0x04)
#define rISP_DIN			IST40XX_DA_ADDR(rISP_BASE | 0x08)
#define rISP_DOUT			IST40XX_DA_ADDR(rISP_BASE | 0x0C)
#define rISP_ISP_EN			IST40XX_DA_ADDR(rISP_BASE | 0x10)
#define rISP_AUTO_READ_CTRL		IST40XX_DA_ADDR(rISP_BASE | 0x14)
#define rISP_CRC			IST40XX_DA_ADDR(rISP_BASE | 0x18)
#define rISP_COMPARE_MODE		IST40XX_DA_ADDR(rISP_BASE | 0x1C)
#define rISP_TMODE1			IST40XX_DA_ADDR(rISP_BASE | 0x30)
#define rISP_STATUS			IST40XX_DA_ADDR(rISP_BASE | 0x90)

// DMA
#define rDMA_BASE			(0x4000A000)
#define rDMA1_CTL			IST40XX_DA_ADDR(rDMA_BASE | 0x010)
#define rDMA1_SRCADDR			IST40XX_DA_ADDR(rDMA_BASE | 0x014)
#define rDMA1_DSTADDR			IST40XX_DA_ADDR(rDMA_BASE | 0x018)

// I2C
#define rI2C_CTRL			IST40XX_DA_ADDR(0x30000000)

// F/W update Info
#define IST40XX_FW_NAME			"ist40xx.fw"
#define IST40XX_BIN_NAME		"ist40xx.bin"
#define IST40XX_IUM_NAME		"ist40xx_ium.bin"

// Update func
#define MASK_UPDATE_INTERNAL		(1)
#define MASK_UPDATE_FW			(2)
#define MASK_UPDATE_SDCARD		(3)
#define MASK_UPDATE_ERASE		(4)

// Version flag
#define FLAG_MAIN			(1)
#define FLAG_FW				(2)
#define FLAG_TEST			(3)

int ist40xx_set_padctrl2(struct ist40xx_data *data);
int ist40xx_set_i2c_32bit(struct ist40xx_data *data);
int ist40xx_isp_enable(struct ist40xx_data *data, bool enable);
int ist40xx_ium_read(struct ist40xx_data *data, u32 *buf32);
int ist40xx_write_sec_info(struct ist40xx_data *data, u8 idx, u32 *buf32,
			   int len);
int ist40xx_read_sec_info(struct ist40xx_data *data, u8 idx, u32 *buf32,
			  int len);
int ist40xx_get_update_info(struct ist40xx_data *data, const u8 *buf,
			    const u32 size);
int ist40xx_get_tsp_info(struct ist40xx_data *data);
void ist40xx_print_info(struct ist40xx_data *data);
u32 ist40xx_parse_ver(struct ist40xx_data *data, int flag, const u8 *buf);
int ist40xx_fw_update(struct ist40xx_data *data, const u8 *buf, int size);
int ist40xx_fw_recovery(struct ist40xx_data *data);
int ist40xx_auto_bin_update(struct ist40xx_data *data);
int ist40xx_miscalib_wait(struct ist40xx_data *data);
int ist40xx_miscalibrate(struct ist40xx_data *data);
int ist40xx_calib_wait(struct ist40xx_data *data);
int ist40xx_calibrate(struct ist40xx_data *data, int wait_cnt);
int ist40xx_init_update_sysfs(struct ist40xx_data *data);

#endif  // __IST40XX_UPDATE_H__
