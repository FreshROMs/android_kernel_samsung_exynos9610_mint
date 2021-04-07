/* arch/arm/mach-exynos/cal_bts.c
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

#include "cal_bts.h"

void bts_setqos(addr_u32 base, unsigned int priority)  //QOS :  [RRRRWWWW]
{
	Outp32(base + BTS_RCON, 0x0);
	Outp32(base + BTS_RCON, 0x0);
	Outp32(base + BTS_WCON, 0x0);
	Outp32(base + BTS_WCON, 0x0);

	Outp32(base + BTS_PRIORITY, ((priority >> 16) & 0xFFFF));
	Outp32(base + BTS_TOKENMAX, 0xFFDF);
	Outp32(base + BTS_BWUPBOUND, 0x18);
	Outp32(base + BTS_BWLOBOUND, 0x1);
	Outp32(base + BTS_INITTKN, 0x8);

	Outp32(base + BTS_PRIORITY + WOFFSET, (priority & 0xFFFF));
	Outp32(base + BTS_TOKENMAX + WOFFSET, 0xFFDF);
	Outp32(base + BTS_BWUPBOUND + WOFFSET, 0x18);
	Outp32(base + BTS_BWLOBOUND + WOFFSET, 0x1);
	Outp32(base + BTS_INITTKN + WOFFSET, 0x8);

	Outp32(base + BTS_RCON, 0x1);
	Outp32(base + BTS_WCON, 0x1);
}

void bts_setqos_bw(addr_u32 base, unsigned int priority,
			unsigned int window, unsigned int token) //QOS :  [RRRRWWWW]
{
	Outp32(base + BTS_RCON, 0x0);
	Outp32(base + BTS_RCON, 0x0);
	Outp32(base + BTS_WCON, 0x0);
	Outp32(base + BTS_WCON, 0x0);

	Outp32(base + BTS_PRIORITY, ((priority >> 16) & 0xFFFF));
	Outp32(base + BTS_TOKENMAX, 0xFFDF);
	Outp32(base + BTS_BWUPBOUND, 0x18);
	Outp32(base + BTS_BWLOBOUND, 0x1);
	Outp32(base + BTS_INITTKN, 0x8);
	Outp32(base + BTS_DEMWIN, window);
	Outp32(base + BTS_DEMTKN, token);
	Outp32(base + BTS_DEFWIN, window);
	Outp32(base + BTS_DEFTKN, token);
	Outp32(base + BTS_PRMWIN, window);
	Outp32(base + BTS_PRMTKN, token);
	Outp32(base + BTS_FLEXIBLE, 0x0);

	Outp32(base + BTS_PRIORITY + WOFFSET, (priority & 0xFFFF));
	Outp32(base + BTS_TOKENMAX + WOFFSET, 0xFFDF);
	Outp32(base + BTS_BWUPBOUND + WOFFSET, 0x18);
	Outp32(base + BTS_BWLOBOUND + WOFFSET, 0x1);
	Outp32(base + BTS_INITTKN + WOFFSET, 0x8);
	Outp32(base + BTS_DEMWIN + WOFFSET, window);
	Outp32(base + BTS_DEMTKN + WOFFSET, token);
	Outp32(base + BTS_DEFWIN + WOFFSET, window);
	Outp32(base + BTS_DEFTKN + WOFFSET, token);
	Outp32(base + BTS_PRMWIN + WOFFSET, window);
	Outp32(base + BTS_PRMTKN + WOFFSET, token);
	Outp32(base + BTS_FLEXIBLE + WOFFSET, 0x0);

	Outp32(base + BTS_RMODE, 0x1);
	Outp32(base + BTS_WMODE, 0x1);
	Outp32(base + BTS_RCON, 0x3);
	Outp32(base + BTS_WCON, 0x3);
}


void bts_setqos_mo(addr_u32 base, unsigned int priority,
			unsigned int rmo, unsigned int wmo)  //QOS :  [RRRRWWWW]
{
	Outp32(base + BTS_RCON, 0x0);
	Outp32(base + BTS_RCON, 0x0);
	Outp32(base + BTS_WCON, 0x0);
	Outp32(base + BTS_WCON, 0x0);

	Outp32(base + BTS_PRIORITY, ((priority >> 16) & 0xFFFF));
	Outp32(base + BTS_PRIORITY + WOFFSET, (priority & 0xFFFF));

	if(rmo < MAX_MO_LIMIT)
	{
		Outp32(base + BTS_MOUPBOUND, 0x7F - rmo);
		Outp32(base + BTS_MOLOBOUND, rmo);
		Outp32(base + BTS_FLEXIBLE, 0x0);
		Outp32(base + BTS_RMODE, 0x2);
		Outp32(base + BTS_RCON, 0x3);
	}
	else
	{
		Outp32(base + BTS_TOKENMAX, 0xFFDF);
		Outp32(base + BTS_BWUPBOUND, 0x18);
		Outp32(base + BTS_BWLOBOUND, 0x1);
		Outp32(base + BTS_INITTKN, 0x8);
		Outp32(base + BTS_RCON, 0x1);
	}

	if(wmo < MAX_MO_LIMIT)
	{
		Outp32(base + BTS_MOUPBOUND + WOFFSET, 0x7F - wmo);
		Outp32(base + BTS_MOLOBOUND + WOFFSET, wmo);
		Outp32(base + BTS_FLEXIBLE + WOFFSET, 0x0);
		Outp32(base + BTS_WMODE, 0x2);
		Outp32(base + BTS_WCON, 0x3);
	}
	else
	{
		Outp32(base + BTS_TOKENMAX + WOFFSET, 0xFFDF);
		Outp32(base + BTS_BWUPBOUND + WOFFSET, 0x18);
		Outp32(base + BTS_BWLOBOUND + WOFFSET, 0x1);
		Outp32(base + BTS_INITTKN + WOFFSET, 0x8);
		Outp32(base + BTS_WCON, 0x1);
	}
}

void bts_setqos_fbmbw(addr_u32 base, unsigned int priority, unsigned int window,
			unsigned int token, unsigned int fbm)  //QOS :  [RRRRWWWW]
{
	Outp32(base + BTS_RCON, 0x0);
	Outp32(base + BTS_RCON, 0x0);
	Outp32(base + BTS_WCON, 0x0);
	Outp32(base + BTS_WCON, 0x0);

	Outp32(base + BTS_PRIORITY, ((priority >> 16) & 0xFFFF));
	Outp32(base + BTS_TOKENMAX, 0xFFDF);
	Outp32(base + BTS_BWUPBOUND, 0x18);
	Outp32(base + BTS_BWLOBOUND, 0x1);
	Outp32(base + BTS_INITTKN, 0x8);
	Outp32(base + BTS_DEMWIN, window);
	Outp32(base + BTS_DEMTKN, token);
	Outp32(base + BTS_DEFWIN, window);
	Outp32(base + BTS_DEFTKN, token);
	Outp32(base + BTS_PRMWIN, window);
	Outp32(base + BTS_PRMTKN, token);
	Outp32(base + BTS_FLEXIBLE, 0x2);

	Outp32(base + BTS_PRIORITY + WOFFSET, (priority & 0xFFFF));
	Outp32(base + BTS_TOKENMAX + WOFFSET, 0xFFDF);
	Outp32(base + BTS_BWUPBOUND + WOFFSET, 0x18);
	Outp32(base + BTS_BWLOBOUND + WOFFSET, 0x1);
	Outp32(base + BTS_INITTKN + WOFFSET, 0x8);
	Outp32(base + BTS_DEMWIN + WOFFSET, window);
	Outp32(base + BTS_DEMTKN + WOFFSET, token);
	Outp32(base + BTS_DEFWIN + WOFFSET, window);
	Outp32(base + BTS_DEFTKN + WOFFSET, token);
	Outp32(base + BTS_PRMWIN + WOFFSET, window);
	Outp32(base + BTS_PRMTKN + WOFFSET, token);
	Outp32(base + BTS_FLEXIBLE + WOFFSET, 0x1);

	Outp32(base + BTS_RMODE, 0x1);
	Outp32(base + BTS_WMODE, 0x1);

	if(fbm == 1) {
		Outp32(base + BTS_RCON, 0x7);
		Outp32(base + BTS_WCON, 0x7);
	} else {
		Outp32(base + BTS_RCON, 0x3);
		Outp32(base + BTS_WCON, 0x3);
	}
}

void bts_disable(addr_u32 base)
{
	/* reset to default */
	Outp32(base + BTS_RCON, 0x0);
	Outp32(base + BTS_RCON, 0x0);
	Outp32(base + BTS_WCON, 0x0);
	Outp32(base + BTS_WCON, 0x0);

	Outp32(base + BTS_RMODE, 0x1);
	Outp32(base + BTS_WMODE, 0x1);

	Outp32(base + BTS_PRIORITY, 0xA942);
	Outp32(base + BTS_TOKENMAX, 0x0);
	Outp32(base + BTS_BWUPBOUND, 0x3FFF);
	Outp32(base + BTS_BWLOBOUND, 0x3FFF);
	Outp32(base + BTS_INITTKN, 0x7FFF);
	Outp32(base + BTS_DEMWIN, 0x7FFF);
	Outp32(base + BTS_DEMTKN, 0x1FFF);
	Outp32(base + BTS_DEFWIN, 0x7FFF);
	Outp32(base + BTS_DEFTKN, 0x1FFF);
	Outp32(base + BTS_PRMWIN, 0x7FFF);
	Outp32(base + BTS_PRMTKN, 0x1FFF);
	Outp32(base + BTS_MOUPBOUND, 0x1F);
	Outp32(base + BTS_MOLOBOUND, 0x1F);
	Outp32(base + BTS_FLEXIBLE, 0x0);

	Outp32(base + BTS_PRIORITY + WOFFSET, 0xA942);
	Outp32(base + BTS_TOKENMAX + WOFFSET, 0x0);
	Outp32(base + BTS_BWUPBOUND + WOFFSET, 0x3FFF);
	Outp32(base + BTS_BWLOBOUND + WOFFSET, 0x3FFF);
	Outp32(base + BTS_INITTKN + WOFFSET, 0x7FFF);
	Outp32(base + BTS_DEMWIN + WOFFSET, 0x7FFF);
	Outp32(base + BTS_DEMTKN + WOFFSET, 0x1FFF);
	Outp32(base + BTS_DEFWIN + WOFFSET, 0x7FFF);
	Outp32(base + BTS_DEFTKN + WOFFSET, 0x1FFF);
	Outp32(base + BTS_PRMWIN + WOFFSET, 0x7FFF);
	Outp32(base + BTS_PRMTKN + WOFFSET, 0x1FFF);
	Outp32(base + BTS_MOUPBOUND + WOFFSET, 0x1F);
	Outp32(base + BTS_MOLOBOUND + WOFFSET, 0x1F);
	Outp32(base + BTS_FLEXIBLE + WOFFSET, 0x0);
}

void bts_settrexqos(addr_u32 base, unsigned int priority)
{
	Outp32(base + TBTS_CON, 0x0);
	Outp32(base + TBTS_RCON, 0x0);
	Outp32(base + TBTS_WCON, 0x0);

#if defined(CONFIG_SOC_EXYNOS7420_EVT_0)
	Outp32(base + TBTS_QURGUPTH, 0xF);
	Outp32(base + TBTS_QURGDOWNTH, 0xF);
#endif
	Outp32(base + TBTS_DEMQOS, priority & 0xF);
	Outp32(base + TBTS_DEFQOS, priority & 0xF);
	Outp32(base + TBTS_PRMQOS, priority & 0xF);

	Outp32(base + TBTS_CON, 0x1);
}

void bts_settrexqos_mo(addr_u32 base, unsigned int priority, unsigned int mo)
{
	Outp32(base + TBTS_CON, 0x0);
	Outp32(base + TBTS_RCON, 0x0);
	Outp32(base + TBTS_WCON, 0x0);

#if defined(CONFIG_SOC_EXYNOS7420_EVT_0)
	Outp32(base + TBTS_QURGUPTH, 0xF);
	Outp32(base + TBTS_QURGDOWNTH, 0xF);
#endif
	Outp32(base + TBTS_TKNUPBOUND, mo);
	Outp32(base + TBTS_TKNLOBOUND, mo);

	Outp32(base + TBTS_DEMTH, mo);
	Outp32(base + TBTS_PRMTH, mo);

	Outp32(base + TBTS_DEMQOS, priority & 0xF);
	Outp32(base + TBTS_DEFQOS, priority & 0xF);
	Outp32(base + TBTS_PRMQOS, priority & 0xF);

	Outp32(base + TBTS_RCON, 0x11);		// MO mode & Blocking on
	Outp32(base + TBTS_WCON, 0x11);
	Outp32(base + TBTS_CON, 0x1);
}

void bts_settrexqos_bw(addr_u32 base, unsigned int priority, unsigned int decval)
{
	Outp32(base + TBTS_CON, 0x0);
	Outp32(base + TBTS_RCON, 0x0);
	Outp32(base + TBTS_WCON, 0x0);

#if defined(CONFIG_SOC_EXYNOS7420_EVT_0)
	Outp32(base + TBTS_QURGUPTH, 0xF);
	Outp32(base + TBTS_QURGDOWNTH, 0xF);
#endif

	Outp32(base + TBTS_TKNUPBOUND, 0x3FFF);
	Outp32(base + TBTS_TKNLOBOUND, 0x3FF7);
	Outp32(base + TBTS_RTKNINIT, 0x3FFF);
	Outp32(base + TBTS_WTKNINIT, 0x3FFF);

	Outp32(base + TBTS_DEMQOS, priority & 0xF);
	Outp32(base + TBTS_DEFQOS, priority & 0xF);
	Outp32(base + TBTS_PRMQOS, priority & 0xF);

	Outp32(base + TBTS_RTKNDEC, decval);
	Outp32(base + TBTS_WTKNDEC, decval);

	Outp32(base + TBTS_RCON, 0x1);		// Blocking on
	Outp32(base + TBTS_WCON, 0x1);
	Outp32(base + TBTS_CON, 0x1);

}

void bts_settrexqos_fbmbw(addr_u32 base, unsigned int priority)
{
	Outp32(base + TBTS_CON, 0x0);
	Outp32(base + TBTS_RCON, 0x0);
	Outp32(base + TBTS_WCON, 0x0);

	Outp32(base + TBTS_DEMQOS, priority & 0xF);
	Outp32(base + TBTS_DEFQOS, priority & 0xF);
	Outp32(base + TBTS_PRMQOS, priority & 0xF);

	Outp32(base + TBTS_RCON, 0x1);		// Blocking on
	Outp32(base + TBTS_WCON, 0x1);
	Outp32(base + TBTS_CON, 0x1);
}

void bts_trexdisable(addr_u32 base)
{
	Outp32(base + TBTS_CON, 0x00010000);
	Outp32(base + TBTS_RCON, 0x0);
	Outp32(base + TBTS_WCON, 0x0);

	Outp32(base + TBTS_TKNUPBOUND, 0x5FFF);
	Outp32(base + TBTS_TKNLOBOUND, 0x1FFF);

	Outp32(base + TBTS_DEMTH, 0x5FFF);
	Outp32(base + TBTS_PRMTH, 0x1FFF);

#if defined(CONFIG_SOC_EXYNOS7420_EVT_0)
	Outp32(base + TBTS_DEMQOS, 0x9);
	Outp32(base + TBTS_DEFQOS, 0x4);
	Outp32(base + TBTS_PRMQOS, 0x2);
#else
	Outp32(base + TBTS_DEMQOS, 0x2);
	Outp32(base + TBTS_DEFQOS, 0x4);
	Outp32(base + TBTS_PRMQOS, 0x9);
#endif

	Outp32(base + TBTS_QURGUPTH, 0x8);
	Outp32(base + TBTS_QURGDOWNTH, 0x8);
	Outp32(base + TBTS_RTKNINIT, 0x3FFF);
	Outp32(base + TBTS_WTKNINIT, 0x3FFF);
	Outp32(base + TBTS_RTKNDEC, 0x0);
	Outp32(base + TBTS_WTKNDEC, 0x0);
}

void bts_setnsp(addr_u32 base, unsigned int nsp)
{
	Outp32(base , nsp);
}
