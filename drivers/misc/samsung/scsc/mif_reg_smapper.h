/****************************************************************************
 *
 * Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __MIF_REG_SMAPPER_H
#define __MIF_REG_SMAPPER_H

/***************************************/
/* SMAPPER v2.0.1 register definitions */
/***************************************/
#define NUM_BANKS_160		4
#define NUM_BANKS_64		7
#define NUM_BANKS		(NUM_BANKS_160 + NUM_BANKS_64)

/* It enables CP_ADDR_MAP */
#define ADDR_MAP_EN_BASE	0x000
#define ADDR_MAP_EN(b)		(ADDR_MAP_EN_BASE + (0x10 * (b)))

/* SRAM write control. Set this register before CP initializes SRAM.
 * If disable this bit, CP cannot acces SRAM by APB I/F. You need to
 * disable ADDR_MAP_CTRL before you set this bit 1'b1 means
 * SRAM write, 1'b0 means SRAM read
 */
#define SRAM_WRITE_CTRL_BASE	0x004
#define SRAM_WRITE_CTRL(b)	(SRAM_WRITE_CTRL_BASE + (0x10 * (b)))

/* It defines the start address of CP virtual addres for 0-31. You
 * need to disable ADDR_MAP_EN before you set this bit
 */
#define START_ADDR_BASE		0x008
#define START_ADDR(b)		(START_ADDR_BASE + (0x10 * (b)))

/* For CP_ADDR_GRANULARITY between 0-31. You
 * need to disable ADDR_MAP_EN before you set this bit
 */
#define ADDR_GRANULARITY_BASE	0x00c
#define ADDR_GRANULARITY(b)	(ADDR_GRANULARITY_BASE + (0x10 * (b)))

/* It defines the MSB part of 36-bit AP phys address [35:0]. It is starting
 * point of access permission.
 */
#define AW_START_ADDR		0x100
/* It defines the MSB part of 36-bit AP phys address [35:0]. It is end
 * point of access permission.
 */
#define AW_END_ADDR		0x104
/* It defines out-of-bound of access windows when it is set to 1'b1 by
 * Access Window
 */
#define AW_ADDR_MAP_STATUS	0x200
#define ORIGIN_ADDR_AR		0x204
#define ORIGIN_ADDR_AW		0x208
/* Read APB bus errors */
#define APB_STATUS_0		0x300
#define APB_STATUS_1		0x304

/* The Q-channel interfaces enable communication to an external
 * power controller
 */
#define SMAPPER_QCH_DISABLE	0x400

/* SRAM r/w addres
 * PWDATA[24:0] is used for 25'b SRAM read/write.
 * Only can access ADDR_MAP_EN is disabled
 */
#define SRAM_BANK_BASE		0x1000

#define SRAM_BANK_INDEX(b, r)    ((SRAM_BANK_BASE + (b * 0x400)) + (4 * (r)))

#define ADDR_MAP_EN_BIT		BIT(0)

#endif /*_MIF_REG_SMAPPER_H*/
