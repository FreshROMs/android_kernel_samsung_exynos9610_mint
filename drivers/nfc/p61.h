/*
 * Copyright (C) 2012-2014 NXP Semiconductors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program
 */

#define DEFAULT_BUFFER_SIZE 258

#define P61_MAGIC 0xEB

#if !defined(CONFIG_NFC_FEATURE_SN100U)
#define P61_SET_PWR _IOW(P61_MAGIC, 0x01, unsigned long)
#define P61_SET_DBG _IOW(P61_MAGIC, 0x02, unsigned long)
#define P61_SET_POLL _IOW(P61_MAGIC, 0x03, unsigned long)

/* To set SPI configurations like gpio, clks */
#define P61_SET_SPI_CONFIG _IO(P61_MAGIC, 0x04)
/* Set the baud rate of SPI master clock nonTZ */
#define P61_ENABLE_SPI_CLK _IO(P61_MAGIC, 0x05)
/* To disable spi core clock */
#define P61_DISABLE_SPI_CLK _IO(P61_MAGIC, 0x06)
/* only nonTZ +++++*/
/* Transmit data to the device and retrieve data from it simultaneously.*/
#define P61_RW_SPI_DATA _IOWR(P61_MAGIC, 0x07, unsigned long)
/* only nonTZ -----*/

/*
 * SPI Request NFCC to enable p61 power, only in param
 * Only for SPI
 * level 1 = Enable power
 * level 0 = Disable power
 */
#define P61_SET_SPM_PWR    _IOW(P61_MAGIC, 0x08, unsigned int)

/* SPI or DWP can call this ioctl to get the current
 * power state of P61
 *
 */
#define P61_GET_SPM_STATUS    _IOR(P61_MAGIC, 0x09, unsigned int)
#define P61_GET_ESE_ACCESS    _IOW(P61_MAGIC, 0x0A, unsigned int)
#define P61_SET_DWNLD_STATUS    _IOW(P61_MAGIC, 0x0B, unsigned long)
#else
#define P61_SET_PWR				_IOW(P61_MAGIC, 0x01, long)
#define P61_SET_DBG				_IOW(P61_MAGIC, 0x02, long)
#define P61_SET_POLL			_IOW(P61_MAGIC, 0x03, long)

/* SPI Request NFCC to enable p61 power, only in param
 * Only for SPI
 * level 1 = Enable power
 * level 0 = Disable power
 */
#define P61_SET_SPM_PWR			_IOW(P61_MAGIC, 0x04, long)

/* SPI or DWP can call this ioctl to get the current
 * power state of P61
 */
#define P61_GET_SPM_STATUS		_IOR(P61_MAGIC, 0x05, long)
#define P61_SET_THROUGHPUT		_IOW(P61_MAGIC, 0x06, long)
#define P61_GET_ESE_ACCESS		_IOW(P61_MAGIC, 0x07, long)
#define P61_SET_POWER_SCHEME	_IOW(P61_MAGIC, 0x08, long)
#define P61_SET_DWNLD_STATUS	_IOW(P61_MAGIC, 0x09, long)
#define P61_INHIBIT_PWR_CNTRL	_IOW(P61_MAGIC, 0x0A, long)

/* only nonTZ +++++*/
/* Transmit data to the device and retrieve data from it simultaneously.*/
#define P61_RW_SPI_DATA			_IOWR(P61_MAGIC, 0x0F, unsigned long)
/* only nonTZ -----*/
#endif

struct p61_ioctl_transfer {
	unsigned char *rx_buffer;
	unsigned char *tx_buffer;
	unsigned int len;
};

struct p61_spi_platform_data {
	unsigned int irq_gpio;
	unsigned int rst_gpio;
};
