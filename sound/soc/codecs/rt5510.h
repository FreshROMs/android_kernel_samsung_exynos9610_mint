/*
 *  sound/soc/codecs/rt5510.h
 *
 *  Copyright (C) 2018 Richtek Technology Inc.
 *  cy_huang <cy_huang@richtek.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __SND_SOC_RT5510_H
#define __SND_SOC_RT5510_H

#include <linux/mutex.h>
#include <linux/rt-regmap.h>

#ifdef CONFIG_SND_RICHTEK_SPM
#include "richtek_spm_cls.h"
#endif /* CONFIG_SND_RICHTEK_SPM */

struct rt5510_chip {
	struct i2c_client *i2c;
	struct device *dev;
	struct snd_soc_codec *codec;
	struct platform_device *param_dev;
	struct rt_regmap_device *regmap;
#ifdef CONFIG_SND_RICHTEK_SPM
	struct richtek_spm_classdev richtek_spm;
#endif /* CONFIG_SND_RICHTEK_SPM */
	struct mutex var_lock;
	u16 chip_rev;
	u8 dev_cnt;
	int pwr_cnt;
};

int rt5510_codec_trigger_param_write(struct rt5510_chip *chip,
				     void *param, int size);
int rt5510_regmap_register(struct rt5510_chip *chip);
void rt5510_regmap_unregister(struct rt5510_chip *chip);

/* reg address divide into two parts [16:8] size, [7:0] addr */
#define RT5510_GET_ADDR(_addr)		((_addr) & 0xff)
#define RT5510_GET_SIZE(_addr)		(((_addr) >> 8) & 0xff)

#define RT5510_REG_DEVID		(0x200)
#define RT5510_REG_SERIAL_DATA_STATUS	(0x101)
#define RT5510_REG_BLOCK_CLK_CTRL	(0x102)
#define RT5510_REG_SYSTEM_CTRL		(0x103)
#define RT5510_REG_IRQ_EN		(0x104)
#define RT5510_REG_IRQ_STATUS1		(0x105)
#define RT5510_REG_CRC_CHECK		(0x106)
#define RT5510_REG_ADDA_CLOCK		(0x107)
#define RT5510_REG_I2C_CTRL		(0x108)
#define RT5510_REG_IRQ_STATUS2		(0x109)
#define RT5510_REG_SERIAL_CFG1		(0x110)
#define RT5510_REG_SERIAL_CFG2		(0x111)
#define RT5510_REG_DATAO_SEL		(0x112)
#define RT5510_REG_TDM_CFG1		(0x113)
#define RT5510_REG_TDM_CFG2		(0x214)
#define RT5510_REG_TDM_CFG3		(0x215)
#define RT5510_REG_HPF_CTRL		(0x118)
#define RT5510_REG_HPF0_COEF		(0x419)
#define RT5510_REG_HPF1_COEF		(0x41A)
#define RT5510_REG_HPF2_COEF		(0x41B)
#define RT5510_REG_GISENS		(0x21C)
#define RT5510_REG_GVSENS		(0x11D)
#define RT5510_REG_PATH_BYPASS		(0x11E)
#define RT5510_REG_WDT_CTRL		(0x120)
#define RT5510_REG_HCLIP_CTRL		(0x224)
#define RT5510_REG_RAMP_CTRL		(0x128)
#define RT5510_REG_VOL_CTRL		(0x129)
#define RT5510_REG_SPS_CTRL		(0x130)
#define RT5510_REG_SG_CFG		(0x231)
#define RT5510_REG_SG_VOMIN		(0x232)
#define RT5510_REG_SIGMAX		(0x233)
#define RT5510_REG_SPSTH		(0x334)
#define RT5510_REG_TC_GAIN_M60		(0x238)
#define RT5510_REG_TC_GAIN_M28		(0x239)
#define RT5510_REG_TC_GAIN_P4		(0x23A)
#define RT5510_REG_TC_GAIN_P36		(0x23B)
#define RT5510_REG_TC_GAIN_P68		(0x23C)
#define RT5510_REG_TC_GAIN_P100		(0x23D)
#define RT5510_REG_TC_GAIN_P132		(0x23E)
#define RT5510_REG_CALI_T0		(0x13F)
#define RT5510_REG_BST_CTRL		(0x140)
#define RT5510_REG_BST_L1		(0x241)
#define RT5510_REG_BST_L2		(0x242)
#define RT5510_REG_BST_L3		(0x243)
#define RT5510_REG_PSM_CTRL		(0x144)
#define RT5510_REG_CCMAX		(0x145)
#define RT5510_REG_PROTECTION_CFG	(0x146)
#define RT5510_REG_VPTAT		(0x247)
#define RT5510_REG_VBAT			(0x148)
#define RT5510_REG_BST_CFG1		(0x349)
#define RT5510_REG_BST_CFG2		(0x24A)
#define RT5510_REG_BST_CFG3		(0x14B)
#define RT5510_REG_DA_GAIN		(0x24C)
#define RT5510_REG_FF_GAIN		(0x24D)
#define RT5510_REG_RLD_COEF1		(0x14E)
#define RT5510_REG_RLD_COEF2		(0x14F)
#define RT5510_REG_AUDIO_IN2_SEL	(0x150)
#define RT5510_REG_SIG_GAIN		(0x151)
#define RT5510_REG_IDAC1_TM		(0x152)
#define RT5510_REG_IDAC2_TM		(0x153)
#define RT5510_REG_IDAC3_TM		(0x154)
#define RT5510_REG_GVPTAT		(0x255)
#define RT5510_REG_IPEAK_LOW		(0x156)
#define RT5510_REG_GPWM_LV		(0x157)
#define RT5510_REG_VBT_SENSE		(0x158)
#define RT5510_REG_VPTAT_ADC_CODE	(0x259)
#define RT5510_REG_VBAT_ADC_CODE	(0x25A)
#define RT5510_REG_SUB_ADC_OFFSET	(0x15B)
#define RT5510_REG_IDAC_TM_CTRL		(0x15C)
#define RT5510_REG_IDAC_TEST_CODE	(0x25D)
#define RT5510_REG_IDAC_MONOTONIC	(0x15E)
#define RT5510_REG_PLL_CFG1		(0x160)
#define RT5510_REG_PLL_CFG2		(0x161)
#define RT5510_REG_PLL_CFG3		(0x162)
#define RT5510_REG_PLL_CFG4		(0x163)
#define RT5510_REG_PLL_RATIO		(0x464)
#define RT5510_REG_DRE_CTRL		(0x168)
#define RT5510_REG_DRE_THDMODE		(0x169)
#define RT5510_REG_DRE_TIMING		(0x56A)
#define RT5510_REG_DRE_CORASE		(0x16B)
#define RT5510_REG_PWM_CTRL		(0x170)
#define RT5510_REG_DC_PROTECT_CTRL	(0x174)
#define RT5510_REG_DITHER_CTRL		(0x178)
#define RT5510_REG_ADC_USB_MODE		(0x17C)
#define RT5510_REG_EFUSE_EN		(0x180)
#define RT5510_REG_EFUSE_CTRL		(0x181)
#define RT5510_REG_EFUSE_MAP		(0x882)
#define RT5510_REG_INTERNAL_CFG		(0x188)
#define RT5510_REG_DC_ADJ		(0x189)
#define RT5510_REG_ZC_CFG		(0x18A)
#define RT5510_REG_BG_CFG		(0x18B)
#define RT5510_REG_TM_EN		(0x190)
#define RT5510_REG_RAM_BIST_TST		(0x191)
#define RT5510_REG_SCAN_MODE		(0x192)
#define RT5510_REG_DBG_OUT_SEL1		(0x193)
#define RT5510_REG_DBG_OUT_SEL2		(0x294)
#define RT5510_REG_RESV0		(0x198)
#define RT5510_REG_RESV1		(0x199)
#define RT5510_REG_RESV2		(0x19A)
#define RT5510_REG_RESV3		(0x19B)
#define RT5510_REG_RESV4		(0x1A0)
#define RT5510_REG_RESV5		(0x1A1)
#define RT5510_REG_RESV6		(0x1A2)
#define RT5510_REG_RESV7		(0x1A3)
#define RT5510_REG_RESV8		(0x1A8)
#define RT5510_REG_RESV9		(0x1A9)
#define RT5510_REG_RESV10		(0x1B0)
#define RT5510_REG_RESV11		(0x1B1)
#define RT5510_REG_RESV12		(0x1B2)
#define RT5510_REG_RESV13		(0x1B3)
#define RT5510_REG_RESV14		(0x1B4)
#define RT5510_REG_RESV15		(0x1B5)
#define RT5510_REG_RESV16		(0x1B6)
#define RT5510_REG_RESV17		(0x2B7)
#define RT5510_REG_RESV18		(0x1B8)
#define RT5510_REG_RESV19		(0x1B9)
#define RT5510_REG_RESV20		(0x1BA)
#define RT5510_REG_RESV21		(0x1BB)
#define RT5510_REG_RESV22		(0x1BC)
#define RT5510_REG_RESV23		(0x2BD)
#define RT5510_REG_RESV24		(0x1BE)
#define RT5510_REG_RESV25		(0x1BF)
#define RT5510_REG_RESV26		(0x1C0)
#define RT5510_REG_RESV27		(0x1C8)
#define RT5510_REG_RESV28		(0x1D0)
#define RT5510_REG_RESV29		(0x1D1)
#define RT5510_REG_RESV30		(0x1D2)
#define RT5510_REG_RESV31		(0x1D3)
#define RT5510_REG_RESV32		(0x1D4)
#define RT5510_REG_RESV33		(0x1D5)
#define RT5510_REG_RESV34		(0x1D6)
#define RT5510_REG_RESV35		(0x1D7)
#define RT5510_REG_RESV36		(0x1D8)
#define RT5510_REG_RESV37		(0x1D9)
#define RT5510_REG_RESV38		(0x1DA)
#define RT5510_REG_RESV39		(0x1DB)
#define RT5510_REG_RESV40		(0x1E0)
#define RT5510_REG_RESV41		(0x1E1)
#define RT5510_REG_RESV42		(0x1E2)
#define RT5510_REG_RESV43		(0x1E3)
#define RT5510_REG_RESV44		(0x1E4)
#define RT5510_REG_RESV45		(0x1E5)
#define RT5510_REG_RESV46		(0x1E6)
#define RT5510_REG_RESV47		(0x1E7)
#define RT5510_REG_RESV48		(0x1E8)
#define RT5510_REG_RESV49		(0x1E9)
#define RT5510_REG_RESV50		(0x1EA)
#define RT5510_REG_RESV51		(0x1EB)
#define RT5510_REG_RESV52		(0x1EC)
#define RT5510_REG_RESV53		(0x1ED)
#define RT5510_REG_RESV54		(0x1EE)
#define RT5510_REG_RESV55		(0x1EF)
#define RT5510_REG_RESV56		(0x1F0)

#endif /* __SND_SOC_RT5510_H */
