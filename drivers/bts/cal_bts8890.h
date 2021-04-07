/* arch/arm/mach-exynos/cal_bts.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * EXYNOS - BTS CAL code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __BTSCAL_H__
#define __BTSCAL_H__

#if defined(CONFIG_EXYNOS8890_BTS)
#include <linux/io.h>

#define Outp32(addr, data)      (__raw_writel(data, addr))
#define Inp32(addr)             (__raw_readl(addr))
typedef void __iomem *addr_u32;

#else
typedef unsigned long u32;
typedef unsigned long addr_u32;
#define Outp32(addr, data) (*(volatile unsigned int *)(addr) = (data))
#define Inp32(addr) (*(volatile unsigned int *)(addr))

#endif

/* for BTS V2.1 Register */
#define BTS_RCON			0x100
#define BTS_RMODE			0x104
#define BTS_WCON			0x180
#define BTS_WMODE			0x184
#define BTS_PRIORITY			0x200
#define BTS_TOKENMAX			0x204
#define BTS_BWUPBOUND			0x20C
#define BTS_BWLOBOUND			0x210
#define BTS_INITTKN			0x214
#define BTS_RSTCLK			0x218
#define BTS_RSTTKN			0x21C
#define BTS_DEMWIN			0x220
#define BTS_DEMTKN			0x224
#define BTS_DEFWIN			0x228
#define BTS_DEFTKN			0x22C
#define BTS_PRMWIN			0x230
#define BTS_PRMTKN			0x234
#define BTS_MOUPBOUND			0x240
#define BTS_MOLOBOUND			0x244
#define BTS_FLEXIBLE			0x280
#define BTS_POLARITY			0x284
#define BTS_FBMGRP0ADRS			0x290
#define BTS_FBMGRP0ADRE			0x294
#define BTS_FBMGRP1ADRS			0x298
#define BTS_FBMGRP1ADRE			0x29C
#define BTS_FBMGRP2ADRS			0x2A0
#define BTS_FBMGRP2ADRE			0x2A4
#define BTS_FBMGRP3ADRS			0x2A8
#define BTS_FBMGRP3ADRE			0x2AC
#define BTS_FBMGRP4ADRS			0x2B0
#define BTS_FBMGRP4ADRE			0x2B4
#define BTS_EMERGENTRID			0x2C0
#define BTS_EMERGENTWID			0x3C0
#define BTS_RISINGTH			0x2C4
#define BTS_FALLINGTH			0x2C8
#define BTS_FALLINGMO			0x2CC
#define BTS_MOCOUNTER			0x2F0
#define BTS_STATUS				0x2F4
#define BTS_BWMONLOW			0x2F8
#define BTS_BWMONUP				0x2FC

#define WOFFSET				0x100

/* for TREX_BTS Register */
#define TBTS_CON				0x000
#define TBTS_TKNUPBOUND			0x010
#define TBTS_TKNLOBOUND			0x014
#define TBTS_LOADUP				0x020
#define TBTS_LOADDOWN			0x024
#define TBTS_LOADBUSY			0x028
#define TBTS_TKNINC0			0x040
#define TBTS_TKNINC1			0x044
#define TBTS_TKNINC2			0x048
#define TBTS_TKNINC3			0x04C
#define TBTS_TKNINC4			0x050
#define TBTS_TKNINC5			0x054
#define TBTS_TKNINC6			0x058
#define TBTS_TKNINC7			0x05C
#define TBTS_DEMTH				0x070
#define TBTS_PRMTH				0x074
#define TBTS_DEMQOS				0x080
#define TBTS_DEFQOS				0x084
#define TBTS_PRMQOS				0x088
#define TBTS_TIMEOUT			0x090
#define TBTS_QURGUPTH			0x0D0
#define TBTS_QURGDOWNTH			0x0D4
#define TBTS_SELIDMASK			0x0E0
#define TBTS_SELIDVAL			0x0E4
#define TBTS_RCON				0x100
#define TBTS_RTKNINIT			0x110
#define TBTS_RTKNDEC			0x120
#define TBTS_WCON				0x200
#define TBTS_WTKNINIT			0x210
#define TBTS_WTKNDEC			0x220
#define TBTS_EMERGENTID			0x2014
#define TBTS_MASK				0x2010

void bts_setqos(addr_u32 base, unsigned int priority, unsigned int master_id);
void bts_setqos_bw(addr_u32 base, unsigned int priority,
			unsigned int window, unsigned int token, unsigned int master_id);
void bts_setqos_mo(addr_u32 base, unsigned int priority, unsigned int mo, unsigned int master_id);
void bts_setqos_fbmbw(addr_u32 base, unsigned int priority, unsigned int window,
			unsigned int token, unsigned int fbm, unsigned int master_id);
void bts_setemergentID(addr_u32 base, unsigned int master_id);
void bts_disable(addr_u32 base, unsigned int master_id);
void bts_settrexqos(addr_u32 base, unsigned int priority, unsigned int master_id, unsigned int mask);
void bts_settrexqos_mo(addr_u32 base, unsigned int priority, unsigned int mo, unsigned int master_id, unsigned int mask);
void bts_settrexqos_mo_rt(addr_u32 base, unsigned int priority, unsigned int mo, unsigned int master_id, unsigned int mask,
			unsigned int time_out, unsigned int bypass_en);
void bts_settrexqos_mo_cp(addr_u32 base, unsigned int priority, unsigned int mo, unsigned int master_id, unsigned int mask,
			unsigned int time_out, unsigned int bypass_en);
void bts_settrexqos_mo_change(addr_u32 base, unsigned int mo);
void bts_settrexqos_urgent_off(addr_u32 base);
void bts_settrexqos_urgent_on(addr_u32 base);
void bts_settrexqos_bw(addr_u32 base, unsigned int priority, unsigned int decval, unsigned int master_id, unsigned int mask);
void bts_settrexqos_fbmbw(addr_u32 base, unsigned int priority, unsigned int master_id, unsigned int mask);
void bts_trexdisable(addr_u32 base, unsigned int master_id, unsigned int mask);
void bts_setnsp(addr_u32 base, unsigned int nsp);

#endif
